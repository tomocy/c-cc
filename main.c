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
static bool in_asm;

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

static bool are_equal(char* a, char* b) {
  return strcmp(a, b) == 0;
}

static bool are_n_equal(char* a, char* b) {
  return strncmp(a, b, strlen(b)) == 0;
}

static char* replace_ext(char* name, char* ext) {
  name = basename(strdup(name));
  char* dot = strrchr(name, '.');
  if (dot) {
    *dot = '\0';
  }

  char* dst = calloc(1, strlen(name) + strlen(ext));
  sprintf(dst, "%s%s", name, ext);
  return dst;
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

static int run_with(int argc, char** argv, char* input, char* output) {
  char** args = calloc(argc + 5 + 1, sizeof(char*));  // the last +1 for NULL termination
  memcpy(args, argv, argc * sizeof(char*));

  args[argc++] = "-exec";

  if (input) {
    args[argc++] = "-exec/input";
    args[argc++] = input;
  }

  if (output) {
    args[argc++] = "-exec/output";
    args[argc++] = output;
  }

  return run_subprocess(argc, args);
}

static int run_assembler(char* input, char* output) {
  char* args[] = {"as", "-c", input, "-o", output, NULL};
  return run_subprocess(5, args);
}

static int assemble(int argc, char** argv, char* input, char* output) {
  char* tmp_filename = create_tmp_file();
  int status = run_with(argc, argv, input, tmp_filename);
  if (status != 0) {
    return status;
  }
  return run_assembler(tmp_filename, output);
}

static int exec() {
  if (!input_filename) {
    error("no input file to exec");
  }

  Token* tokens = tokenize();
  TopLevelObj* codes = parse(tokens);
  gen(codes);
  return 0;
}

static int run_to_exec(int argc, char** argv) {
  if (!input_filenames) {
    error("no input files");
  }
  if (output_filename && input_filenames->next) {
    error("cannot specify '-o' with multiple filenames");
  }

  atexit(unlink_tmp_file);

  for (Str* input = input_filenames; input; input = input->next) {
    char* output = output_filename;
    if (!output) {
      char* ext = in_asm ? ".s" : ".o";
      output = replace_ext(input->data, ext);
    }

    int status = !in_asm ? assemble(argc, argv, input->data, output) : run_with(argc, argv, input->data, output);
    if (status != 0) {
      return status;
    }
  }

  return 0;
}

int main(int argc, char** argv) {
  parse_args(argc, argv);
  return !do_exec ? run_to_exec(argc, argv) : exec();
}