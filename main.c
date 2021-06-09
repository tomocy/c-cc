#include "cc.h"

typedef struct Str Str;

struct Str {
  Str* next;
  char* data;
};

char* input_filename;
char* output_filename;
static Str* tmp_filenames;
static Str* input_filenames;
static bool do_log_args;
static bool do_exec;
static bool in_obj;
static bool in_asm;

static char* lib_path = "/usr/lib/x86_64-linux-gnu";
static char* gcc_lib_path = "/usr/lib/gcc/x86_64-linux-gnu/9";

static bool are_equal(char* a, char* b) {
  return strcmp(a, b) == 0;
}

static bool are_n_equal(char* a, char* b) {
  return strncmp(a, b, strlen(b)) == 0;
}

static char* format(char* fmt, ...) {
  char* dst = NULL;
  size_t dst_len = 0;
  FILE* out = open_memstream(&dst, &dst_len);

  va_list args;
  va_start(args, fmt);
  vfprintf(out, fmt, args);
  va_end(args);
  fclose(out);
  return dst;
}

static char* replace_ext(char* name, char* ext) {
  name = basename(strdup(name));
  char* dot = strrchr(name, '.');
  if (dot) {
    *dot = '\0';
  }

  return format("%s%s", name, ext);
}

static void add_str(Str** strs, Str* str) {
  str->next = *strs;
  *strs = str;
}

static void add_tmp_filename(Str* fname) {
  add_str(&tmp_filenames, fname);
}

static void add_input_filename(Str* fname) {
  add_str(&input_filenames, fname);
}

static Str* new_str(char* data) {
  Str* str = calloc(1, sizeof(Str));
  str->data = data;
  return str;
}

static char** new_args(int* argc, Str* strs) {
  int len = 0;
  for (Str* arg = strs; arg; arg = arg->next) {
    len++;
  }

  int i = 0;
  char** args = calloc(len + 1, sizeof(char*));
  for (Str* arg = strs; arg; arg = arg->next) {
    args[i++] = arg->data;
  }
  args[i] = NULL;

  *argc = len;
  return args;
}

static char* create_tmp_file() {
  char* name = strdup("/tmp/cc-XXXXXX");
  int fd = mkstemp(name);
  if (fd == -1) {
    fprintf(stderr, "failed to create a tmp file: %s\n", strerror(errno));
  }
  close(fd);

  add_tmp_filename(new_str(name));

  return name;
}

static void unlink_tmp_file() {
  for (Str* fname = tmp_filenames; fname; fname = fname->next) {
    unlink(fname->data);
  }
}

static void usage(int status) {
  fprintf(stderr, "Usage: cc [ -o output ] input [rest_inputs...]\n");
  exit(status);
}

static void parse_args(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    if (are_equal(argv[i], "--help")) {
      usage(0);
    }

    if (are_equal(argv[i], "-###")) {
      do_log_args = true;
      continue;
    }

    if (are_equal(argv[i], "-exec")) {
      do_exec = true;
      continue;
    }
    if (are_equal(argv[i], "-exec/input")) {
      input_filename = argv[++i];
      continue;
    }
    if (are_equal(argv[i], "-exec/output")) {
      output_filename = argv[++i];
      continue;
    }

    if (are_equal(argv[i], "-c")) {
      in_obj = true;
      continue;
    }

    if (are_equal(argv[i], "-S")) {
      in_asm = true;
      continue;
    }

    // -o output_filename
    if (are_equal(argv[i], "-o")) {
      if (!argv[++i]) {
        usage(1);
      }
      output_filename = argv[i];
      continue;
    }
    // -o=output_filename etc
    if (are_n_equal(argv[i], "-o")) {
      output_filename = argv[i] + 2;
      continue;
    }

    if (argv[i][0] == '-' && argv[i][1] != '\0') {
      error("unknown argument: %s", argv[i]);
    }

    add_input_filename(new_str(argv[i]));
  }
}

static int run_subprocess(int argc, char** argv) {
  if (do_log_args) {
    fprintf(stderr, "%s", argv[0]);
    for (int i = 1; i < argc; i++) {
      fprintf(stderr, " %s", argv[i]);
    }
    fprintf(stderr, "\n");
  }

  int pid = fork();
  if (pid == -1) {
    fprintf(stderr, "failed to fork to run subprocess: %s\n", strerror(errno));
    return 1;
  }
  if (pid == 0) {
    execvp(argv[0], argv);
    fprintf(stderr, "failed to exec in subprocess: %s: %s\n", argv[0], strerror(errno));
    return 1;
  }

  int status = 0;
  while (wait(&status) > 0) {}
  return status == 0 ? 0 : 1;
}

static int compile(char* input, char* output) {
  char* args[] = {"/proc/self/exe", "-exec", "-exec/input", input, "-exec/output", output, NULL};
  return run_subprocess(6, args);
}

static int assemble(char* input, char* output) {
  char* tmp_fname = create_tmp_file();
  int status = compile(input, tmp_fname);
  if (status != 0) {
    return status;
  }

  char* args[] = {"as", "-c", tmp_fname, "-o", output, NULL};
  return run_subprocess(5, args);
}

static int linkk(Str* inputs, char* output) {
  Str head_link_inputs = {};
  Str* link_inputs = &head_link_inputs;
  for (Str* input = inputs; input; input = input->next) {
    char* tmp_fname = create_tmp_file();
    int status = assemble(input->data, tmp_fname);
    if (status != 0) {
      return status;
    }

    link_inputs = link_inputs->next = new_str(tmp_fname);
  }

  Str head_ld_args = {};
  Str* ld_args = &head_ld_args;
  ld_args = ld_args->next = new_str("ld");
  ld_args = ld_args->next = new_str("-o");
  ld_args = ld_args->next = new_str(output);
  ld_args = ld_args->next = new_str("-m");
  ld_args = ld_args->next = new_str("elf_x86_64");
  ld_args = ld_args->next = new_str("-dynamic-linker");
  ld_args = ld_args->next = new_str("/lib64/ld-linux-x86-64.so.2");
  ld_args = ld_args->next = new_str(format("%s/crt1.o", lib_path));
  ld_args = ld_args->next = new_str(format("%s/crti.o", lib_path));
  ld_args = ld_args->next = new_str(format("%s/crtbegin.o", gcc_lib_path));
  ld_args = ld_args->next = new_str(format("-L%s", gcc_lib_path));
  ld_args = ld_args->next = new_str(format("-L%s", lib_path));
  ld_args = ld_args->next = new_str(format("-L%s/..", lib_path));
  ld_args = ld_args->next = new_str("-L/usr/lib64");
  ld_args = ld_args->next = new_str("-L/lib64");
  ld_args = ld_args->next = new_str("-L/usr/lib/x86_64-linux-gnu");
  ld_args = ld_args->next = new_str("-L/usr/lib/x86_64-pc-linux-gnu");
  ld_args = ld_args->next = new_str("-L/usr/lib/x86_64-redhat-linux");
  ld_args = ld_args->next = new_str("-L/usr/lib");
  ld_args = ld_args->next = new_str("-L/lib");
  for (Str* input = head_link_inputs.next; input; input = input->next) {
    ld_args = ld_args->next = input;
  }
  ld_args = ld_args->next = new_str("-lc");
  ld_args = ld_args->next = new_str("-lgcc");
  ld_args = ld_args->next = new_str("--as-needed");
  ld_args = ld_args->next = new_str("-lgcc_s");
  ld_args = ld_args->next = new_str("--no-as-needed");
  ld_args = ld_args->next = new_str(format("%s/crtend.o", gcc_lib_path));
  ld_args = ld_args->next = new_str(format("%s/crtn.o", lib_path));

  int argc = 0;
  char** args = new_args(&argc, head_ld_args.next);

  return run_subprocess(argc, args);
}

static int exec() {
  if (!input_filename) {
    error("no input file to exec");
  }

  Token* tokens = tokenize();
  tokens = preprocess(tokens);
  TopLevelObj* codes = parse(tokens);
  gen(codes);
  return 0;
}

static int run(int argc, char** argv) {
  if (!input_filenames) {
    error("no input files");
  }
  if (input_filenames->next && output_filename && (in_obj || in_asm)) {
    error("cannot specify '-o' with '-c' or '-S' with multiple files");
  }

  atexit(unlink_tmp_file);

  Str head_link_inputs = {};
  Str* link_inputs = &head_link_inputs;
  for (Str* input = input_filenames; input; input = input->next) {
    char* output = output_filename;
    if (!output) {
      char* ext = in_asm ? ".s" : ".o";
      output = replace_ext(input->data, ext);
    }

    // compile and assemble
    if (in_obj) {
      int status = assemble(input->data, output);
      if (status != 0) {
        return status;
      }
      continue;
    }

    // compile
    if (in_asm) {
      int status = compile(input->data, output);
      if (status != 0) {
        return status;
      }
      continue;
    }

    // Keep the input filename to compile, assemble and link later
    link_inputs = link_inputs->next = new_str(input->data);
  }

  return head_link_inputs.next ? linkk(head_link_inputs.next, output_filename ? output_filename : "a.out") : 0;
}

int main(int argc, char** argv) {
  parse_args(argc, argv);
  return !do_exec ? run(argc, argv) : exec();
}