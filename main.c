#include "cc.h"

Str* include_paths;
// the location where this program lives is kept
// so that the include path relative to the location can be resolved
static char* location;
static char* input_filename;
static char* output_filename;
static Str* tmp_filenames;
static Str* input_filenames;
static bool do_log_args;
static bool do_exec;
static bool in_obj;
static bool in_asm;
static bool in_c;

static char* lib_path = "/usr/lib/x86_64-linux-gnu";
static char* gcc_lib_path = "/usr/lib/gcc/x86_64-linux-gnu/9";

static void add_include_path(Str* path) {
  add_str(&include_paths, path);
}

static void add_tmp_filename(Str* fname) {
  add_str(&tmp_filenames, fname);
}

static void add_input_filename(Str* fname) {
  add_str(&input_filenames, fname);
}

static char* create_tmp_file() {
  char* name = new_tmp_file();
  add_tmp_filename(new_str(name));
  return name;
}

static void unlink_tmp_files() {
  unlink_files(tmp_filenames);
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

static void usage(int status) {
  fprintf(stderr, "Usage: cc [ -o output ] input [rest_inputs...]\n");
  exit(status);
}

static char* take_arg(Str** args, char* arg) {
  if (!arg) {
    usage(1);
  }

  *args = (*args)->next = new_str(arg);
  return arg;
}

static void define_arg_macro(char* arg) {
  char* eq = strchr(arg, '=');
  if (eq) {
    define_builtin_macro(strndup(arg, eq - arg), eq + 1);
  } else {
    define_builtin_macro(strdup(arg), "1");
  }
}

static Str* parse_args(int argc, char** argv) {
  Str head = {};
  Str* cur = &head;

  cur = cur->next = new_str(argv[0]);
  location = dir(argv[0]);

  for (int i = 1; i < argc; i++) {
    cur = cur->next = new_str(argv[i]);

    if (equal_to_str(argv[i], "--help")) {
      usage(0);
    }

    if (equal_to_str(argv[i], "-###")) {
      do_log_args = true;
      continue;
    }

    if (equal_to_str(argv[i], "-exec")) {
      do_exec = true;
      continue;
    }

    if (equal_to_str(argv[i], "-exec/input")) {
      input_filename = argv[++i];
      continue;
    }

    if (equal_to_str(argv[i], "-exec/output")) {
      output_filename = argv[++i];
      continue;
    }

    if (equal_to_str(argv[i], "-c")) {
      in_obj = true;
      continue;
    }

    if (equal_to_str(argv[i], "-S")) {
      in_asm = true;
      continue;
    }

    if (equal_to_str(argv[i], "-E")) {
      in_c = true;
      continue;
    }

    // -o output_filename
    if (equal_to_str(argv[i], "-o")) {
      output_filename = take_arg(&cur, argv[++i]);
      continue;
    }
    // -o=output_filename etc
    if (start_with(argv[i], "-o")) {
      output_filename = argv[i] + 2;
      continue;
    }

    // -I include_path
    if (equal_to_str(argv[i], "-I")) {
      add_include_path(new_str(take_arg(&cur, argv[++i])));
      continue;
    }
    // -I=include_path etc
    if (start_with(argv[i], "-I")) {
      add_include_path(new_str(argv[i] + 2));
      continue;
    }

    // -D name=body
    if (equal_to_str(argv[i], "-D")) {
      define_arg_macro(take_arg(&cur, argv[++i]));
      continue;
    }
    // -D=name=body
    if (start_with(argv[i], "-D")) {
      define_arg_macro(take_arg(&cur, argv[i] + 2));
      continue;
    }

    // -U name=body
    if (equal_to_str(argv[i], "-U")) {
      undefine_macro(take_arg(&cur, argv[++i]));
      continue;
    }
    // -U=name=body
    if (start_with(argv[i], "-U")) {
      undefine_macro(take_arg(&cur, argv[i] + 2));
      continue;
    }

    if (start_with(argv[i], "-") && !equal_to_str(argv[i], "-")) {
      error("unknown argument: %s", argv[i]);
    }

    add_input_filename(new_str(argv[i]));
  }

  return head.next;
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

static int preprocesss(Str* original, char* input, char* output) {
  Str head = {};
  Str* cur = &head;
  for (Str* arg = original; arg; arg = arg->next) {
    cur = cur->next = arg;
  }
  cur = cur->next = new_str("-E");
  cur = cur->next = new_str("-exec");
  cur = cur->next = new_str("-exec/input");
  cur = cur->next = new_str(input);
  if (output) {
    cur = cur->next = new_str("-exec/output");
    cur = cur->next = new_str(output);
  }

  int argc = 0;
  char** args = new_args(&argc, head.next);
  return run_subprocess(argc, args);
}

static int compile(Str* original, char* input, char* output) {
  Str head = {};
  Str* cur = &head;
  for (Str* arg = original; arg; arg = arg->next) {
    cur = cur->next = arg;
  }
  cur = cur->next = new_str("-exec");
  cur = cur->next = new_str("-exec/input");
  cur = cur->next = new_str(input);
  cur = cur->next = new_str("-exec/output");
  cur = cur->next = new_str(output);

  int argc = 0;
  char** args = new_args(&argc, head.next);
  return run_subprocess(argc, args);
}

static int assemble(Str* original, char* input, char* output) {
  char* tmp_fname = create_tmp_file();
  int status = compile(original, input, tmp_fname);
  if (status != 0) {
    return status;
  }

  char* args[] = {"as", "-c", tmp_fname, "-o", output, NULL};
  return run_subprocess(5, args);
}

static int linkk(Str* original, Str* inputs, char* output) {
  Str head_link_inputs = {};
  Str* link_inputs = &head_link_inputs;
  for (Str* input = inputs; input; input = input->next) {
    char* tmp_fname = create_tmp_file();
    int status = assemble(original, input->data, tmp_fname);
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

static void add_default_include_paths() {
  // In order to give priority to user specified files over default ones,
  // add the user specified files after the default ones
  Str* paths = include_paths;
  include_paths = NULL;
  add_include_path(new_str("/usr/include"));
  add_include_path(new_str("/usr/include/x86_64-linux-gnu"));
  add_include_path(new_str("/usr/local/include"));
  add_include_path(new_str(format("%s/include", location)));
  for (Str* path = paths; path; path = path->next) {
    add_include_path(copy_str(path));
  }
}

static int exec(void) {
  if (!input_filename) {
    error("no input file to exec");
  }

  add_default_include_paths();

  Token* tokens = tokenize(input_filename);
  tokens = preprocess(tokens);
  if (in_c) {
    print_tokens(output_filename, tokens);
    return 0;
  }
  TopLevelObj* codes = parse(tokens);
  gen(output_filename, codes);
  return 0;
}

static int run(Str* original) {
  if (!input_filenames) {
    error("no input files");
  }
  if (input_filenames->next && output_filename && (in_obj || in_asm || in_c)) {
    error("cannot specify '-o' with '-c', '-S' or '-E' with multiple files");
  }

  atexit(unlink_tmp_files);

  Str head_link_inputs = {};
  Str* link_inputs = &head_link_inputs;
  for (Str* input = input_filenames; input; input = input->next) {
    char* output = output_filename;
    if (!output) {
      char* ext = in_asm ? ".s" : ".o";
      output = replace_file_ext(input->data, ext);
    }

    // preprocess, compile and assemble
    if (in_obj) {
      int status = assemble(original, input->data, output);
      if (status != 0) {
        return status;
      }
      continue;
    }

    // preprocess and compile
    if (in_asm) {
      int status = compile(original, input->data, output);
      if (status != 0) {
        return status;
      }
      continue;
    }

    if (in_c) {
      int status = preprocesss(original, input->data, output_filename ? output_filename : NULL);
      if (status != 0) {
        return status;
      }
      continue;
    }

    // Keep the input filename to compile, assemble and link later
    link_inputs = link_inputs->next = new_str(input->data);
  }

  return head_link_inputs.next ? linkk(original, head_link_inputs.next, output_filename ? output_filename : "a.out")
                               : 0;
}

int main(int argc, char** argv) {
  // -D option and -U can define and undefine macros and they take precedence over builtin ones,
  // so it should be before parsing args to define builtin macros.
  define_builtin_macros();

  Str* args = parse_args(argc, argv);
  return !do_exec ? run(args) : exec();
}