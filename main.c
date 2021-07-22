#include "cc.h"

typedef enum {
  FILE_NONE,
  FILE_C,
  FILE_ASM,
  FILE_OBJ,
  FILE_AR,
  FILE_DSO,
} FileType;

// The location where this program lives is kept
// so that the include path relative to the location can be resolved.
static char* location;
char* input_filename;
static char* output_filename;
static Str* tmp_filenames;
static StrQueue inputs;
static StrQueue included_paths;
StrQueue include_paths;
StrQueue std_include_paths;
static bool as_static;
static bool as_shared;
static StrQueue link_args;
static bool do_log_args;
static bool do_exec;
static FileType input_file_type = FILE_NONE;
static bool in_obj;
static bool in_asm;
static bool in_c;
bool common_symbols_enabled = true;
bool as_pic;
static bool do_print_deps;
static bool do_print_deps_and_continue;
static bool do_print_header_deps;
static bool do_print_std_deps = true;
static char* deps_target;
static char* deps_output_filename;
static char* deps_output_file_ext;

static char* lib_path = "/usr/lib/x86_64-linux-gnu";
static char* gcc_lib_path = "/usr/lib/gcc/x86_64-linux-gnu/9";

static char* create_tmp_file() {
  char* name = new_tmp_file();
  add_str(&tmp_filenames, new_str(name));
  return name;
}

static void unlink_tmp_files() {
  unlink_files(tmp_filenames);
}

static char** new_args_from_strs(int* argc, Str* strs) {
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

static char* take_arg(StrQueue* args, char* arg) {
  if (!arg) {
    usage(1);
  }

  push_str(args, new_str(arg));
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

static FileType parse_file_type(char* type) {
  if (equal_to_str(type, "none")) {
    return FILE_NONE;
  }
  if (equal_to_str(type, "c")) {
    return FILE_C;
  }
  if (equal_to_str(type, "assembler")) {
    return FILE_ASM;
  }

  error("unknown argument for -x: %s", type);
  return 0;
}

static char* escape_makefile_target(char* target) {
  char* escaped = calloc(strlen(target) * 2 + 1, sizeof(char));

  for (int src = 0, dst = 0; target[src]; src++) {
    switch (target[src]) {
      case '$':
        escaped[dst++] = '$';
        escaped[dst++] = '$';
        continue;
      case '#':
        escaped[dst++] = '\\';
        escaped[dst++] = '#';
        continue;
      case ' ':
      case '\t':
        for (int i = src - 1; i >= 0 && i == '\\'; i--) {
          escaped[dst++] = '\\';
        }
        escaped[dst++] = '\\';
        escaped[dst++] = target[src];
        continue;
      default:
        escaped[dst++] = target[src];
        continue;
    }
  }

  return escaped;
}

static Str* parse_args(int argc, char** argv) {
  StrQueue args = {};

  StrQueue include_later_paths = {};

  push_str(&args, new_str(argv[0]));
  location = dir(argv[0]);

  for (int i = 1; i < argc; i++) {
    push_str(&args, new_str(argv[i]));

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
      input_file_type = FILE_C;
      continue;
    }

    if (equal_to_str(argv[i], "-o")) {
      output_filename = take_arg(&args, argv[++i]);
      continue;
    }
    if (start_with(argv[i], "-o")) {
      output_filename = argv[i] + 2;
      continue;
    }

    if (equal_to_str(argv[i], "-include")) {
      push_str(&included_paths, new_str(take_arg(&args, argv[++i])));
      continue;
    }

    if (equal_to_str(argv[i], "-I")) {
      push_str(&include_paths, new_str(take_arg(&args, argv[++i])));
      continue;
    }
    if (start_with(argv[i], "-I")) {
      push_str(&include_paths, new_str(argv[i] + 2));
      continue;
    }

    if (equal_to_str(argv[i], "-idirafter")) {
      push_str(&include_later_paths, new_str(take_arg(&args, argv[++i])));
      continue;
    }

    if (equal_to_str(argv[i], "-D")) {
      define_arg_macro(take_arg(&args, argv[++i]));
      continue;
    }
    if (start_with(argv[i], "-D")) {
      define_arg_macro(take_arg(&args, argv[i] + 2));
      continue;
    }

    if (equal_to_str(argv[i], "-U")) {
      undefine_macro(take_arg(&args, argv[++i]));
      continue;
    }
    if (start_with(argv[i], "-U")) {
      undefine_macro(take_arg(&args, argv[i] + 2));
      continue;
    }

    if (equal_to_str(argv[i], "-fcommon")) {
      common_symbols_enabled = true;
      continue;
    }

    if (equal_to_str(argv[i], "-fno-common")) {
      common_symbols_enabled = false;
      continue;
    }

    if (equal_to_str(argv[i], "-x")) {
      input_file_type = parse_file_type(take_arg(&args, argv[++i]));
      continue;
    }
    if (start_with(argv[i], "-x")) {
      input_file_type = parse_file_type(take_arg(&args, argv[i] + 2));
      continue;
    }

    if (start_with(argv[i], "-static")) {
      as_static = true;
      push_str(&link_args, new_str(take_arg(&args, argv[i])));
      continue;
    }

    if (start_with(argv[i], "-shared")) {
      as_shared = true;
      push_str(&link_args, new_str(take_arg(&args, argv[i])));
      continue;
    }

    if (equal_to_str(argv[i], "-pthread")) {
      push_str(&inputs, new_str("-lpthread"));
      continue;
    }

    if (start_with(argv[i], "-l")) {
      push_str(&inputs, new_str(argv[i]));
      continue;
    }

    if (equal_to_str(argv[i], "-L")) {
      push_str(&link_args, new_str("-L"));
      push_str(&link_args, new_str(take_arg(&args, argv[++i])));
      continue;
    }
    if (start_with(argv[i], "-L")) {
      push_str(&link_args, new_str("-L"));
      push_str(&link_args, new_str(argv[i] + 2));
      continue;
    }

    // -Wl,a,b,c
    if (start_with(argv[i], "-Wl,")) {
      push_str(&inputs, new_str(argv[i]));
      continue;
    }

    if (equal_to_str(argv[i], "-Xlinker")) {
      push_str(&link_args, new_str(take_arg(&args, argv[++i])));
      continue;
    }

    if (equal_to_str(argv[i], "-s")) {
      push_str(&link_args, new_str(take_arg(&args, argv[i])));
      continue;
    }

    if (equal_to_str(argv[i], "-M")) {
      in_c = true;
      do_print_deps = true;
      continue;
    }

    if (equal_to_str(argv[i], "-MD") || equal_to_str(argv[i], "-MMD")) {
      do_print_deps_and_continue = true;
      do_print_std_deps = !equal_to_str(argv[i], "-MMD");
      deps_output_file_ext = ".d";
      continue;
    }

    if (equal_to_str(argv[i], "-MT") || equal_to_str(argv[i], "-MQ")) {
      char* target = take_arg(&args, argv[++i]);
      if (equal_to_str(argv[i - 1], "-MQ")) {
        target = escape_makefile_target(target);
      }

      if (deps_target) {
        deps_target = format("%s %s", deps_target, target);
      } else {
        deps_target = target;
      }
      continue;
    }

    if (equal_to_str(argv[i], "-MP")) {
      do_print_header_deps = true;
      continue;
    }

    if (equal_to_str(argv[i], "-MF")) {
      deps_output_filename = take_arg(&args, argv[++i]);
      continue;
    }

    if (equal_to_str(argv[i], "-fpic") || equal_to_str(argv[i], "-fPIC")) {
      as_pic = true;
      continue;
    }

    // Ignore those options for now
    if (start_with_any(argv[i], "-O", "-W", "-g", "-std", "-ffreestanding", "-fno-builtin", "-fno-omit-frame-pointer",
          "-fno-stack-protector", "-fno-strict-aliasing", "-m64", "-mno-red-zone", "-w", NULL)) {
      continue;
    }

    // Test map.c
    if (equal_to_str(argv[i], "-test/map")) {
      test_map();
      continue;
    }

    if (start_with(argv[i], "-") && !equal_to_str(argv[i], "-")) {
      error("unknown argument: %s", argv[i]);
    }

    push_str(&inputs, new_str(argv[i]));
  }

  push_strs(&include_paths, include_later_paths.head.next);

  return args.head.next;
}

static void log_args(int argc, char** argv) {
  if (!do_log_args) {
    return;
  }

  fprintf(stderr, "%s", argv[0]);
  for (int i = 1; i < argc; i++) {
    fprintf(stderr, " %s", argv[i]);
  }
  fprintf(stderr, "\n");
}

static int run_subprocess(int argc, char** argv) {
  log_args(argc, argv);

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
  StrQueue args = {};

  push_strs(&args, original);

  push_str(&args, new_str("-E"));
  push_str(&args, new_str("-exec"));
  push_str(&args, new_str("-exec/input"));
  push_str(&args, new_str(input));

  if (output) {
    push_str(&args, new_str("-exec/output"));
    push_str(&args, new_str(output));
  }

  int argc = 0;
  char** argv = new_args_from_strs(&argc, args.head.next);
  return run_subprocess(argc, argv);
}

static int drive_to_compile(Str* original, char* input, char* output) {
  StrQueue args = {};

  push_strs(&args, original);

  push_str(&args, new_str("-exec"));
  push_str(&args, new_str("-exec/input"));
  push_str(&args, new_str(input));
  push_str(&args, new_str("-exec/output"));
  push_str(&args, new_str(output));

  int argc = 0;
  char** argv = new_args_from_strs(&argc, args.head.next);
  return run_subprocess(argc, argv);
}

static int run_assembler(char* input, char* output) {
  char* args[] = {"as", "-c", input, "-o", output, NULL};
  return run_subprocess(5, args);
}

static int drive_to_assemble(Str* original, char* input, char* output) {
  char* tmp_fname = create_tmp_file();
  int status = drive_to_compile(original, input, tmp_fname);
  if (status != 0) {
    return status;
  }

  return run_assembler(tmp_fname, output);
}

static int drive_to_link(Str* original, Str* inputs, char* output) {
  StrQueue link_inputs = {};
  for (Str* input = inputs; input; input = input->next) {
    if (!equal_to_str(input->data, "-") && !end_with(input->data, ".c")) {
      push_str(&link_inputs, input);
      continue;
    }

    char* tmp_fname = create_tmp_file();
    int status = drive_to_assemble(original, input->data, tmp_fname);
    if (status != 0) {
      return status;
    }

    push_str(&link_inputs, new_str(tmp_fname));
  }

  StrQueue ld_args = {};
  push_str(&ld_args, new_str("ld"));
  push_str(&ld_args, new_str("-o"));
  push_str(&ld_args, new_str(output));
  push_str(&ld_args, new_str("-m"));
  push_str(&ld_args, new_str("elf_x86_64"));
  push_str(&ld_args, new_str("-dynamic-linker"));
  push_str(&ld_args, new_str("/lib64/ld-linux-x86-64.so.2"));

  if (as_shared) {
    push_str(&ld_args, new_str(format("%s/crti.o", lib_path)));
    push_str(&ld_args, new_str(format("%s/crtbeginS.o", gcc_lib_path)));
  } else {
    push_str(&ld_args, new_str(format("%s/crt1.o", lib_path)));
    push_str(&ld_args, new_str(format("%s/crti.o", lib_path)));
    push_str(&ld_args, new_str(format("%s/crtbegin.o", gcc_lib_path)));
  }

  push_str(&ld_args, new_str(format("-L%s", gcc_lib_path)));
  push_str(&ld_args, new_str("-L/usr/lib64"));
  push_str(&ld_args, new_str("-L/lib64"));
  push_str(&ld_args, new_str("-L/usr/lib/x86_64-linux-gnu"));
  push_str(&ld_args, new_str("-L/usr/lib/x86_64-pc-linux-gnu"));
  push_str(&ld_args, new_str("-L/usr/lib/x86_64-redhat-linux"));
  push_str(&ld_args, new_str("-L/usr/lib"));
  push_str(&ld_args, new_str("-L/lib"));

  if (!as_static) {
    push_str(&ld_args, new_str("-dynamic-linker"));
    push_str(&ld_args, new_str("/lib64/ld-linux-x86-64.so.2"));
  }

  push_strs(&ld_args, link_args.head.next);
  push_strs(&ld_args, link_inputs.head.next);

  if (as_static) {
    push_str(&ld_args, new_str("--start-group"));
    push_str(&ld_args, new_str("-lc"));
    push_str(&ld_args, new_str("-lgcc"));
    push_str(&ld_args, new_str("-lgcc_eh"));
    push_str(&ld_args, new_str("--end-group"));
  } else {
    push_str(&ld_args, new_str("-lc"));
    push_str(&ld_args, new_str("-lgcc"));
    push_str(&ld_args, new_str("--as-needed"));
    push_str(&ld_args, new_str("-lgcc_s"));
    push_str(&ld_args, new_str("--no-as-needed"));
  }

  if (as_shared) {
    push_str(&ld_args, new_str(format("%s/crtendS.o", gcc_lib_path)));
  } else {
    push_str(&ld_args, new_str(format("%s/crtend.o", gcc_lib_path)));
  }

  push_str(&ld_args, new_str(format("%s/crtn.o", lib_path)));

  int argc = 0;
  char** args = new_args_from_strs(&argc, ld_args.head.next);

  return run_subprocess(argc, args);
}

static void add_default_include_paths() {
  push_str(&include_paths, new_str(format("%s/include", location)));
  push_str(&include_paths, new_str("/usr/local/include"));
  push_str(&include_paths, new_str("/usr/include/x86_64-linux-gnu"));
  push_str(&include_paths, new_str("/usr/include"));

  for (Str* path = include_paths.head.next; path; path = path->next) {
    push_str(&std_include_paths, copy_str(path));
  }
}

static Str* concat_input_filenames() {
  StrQueue fnames = {};

  for (Str* path = included_paths.head.next; path; path = path->next) {
    char* compensated = compensate_include_filename(path->data, NULL);
    push_str(&fnames, new_str(compensated));
  }

  push_str(&fnames, new_str(input_filename));

  return fnames.head.next;
}

static int exec(void) {
  if (!input_filename) {
    error("no input file to exec");
  }

  add_default_include_paths();

  Str* inputs = concat_input_filenames();
  Token* tokens = tokenize_all(inputs);

  tokens = preprocess(tokens);

  if (do_print_deps || do_print_deps_and_continue) {
    char* output = deps_output_filename ?: output_filename;
    if (!deps_output_filename && deps_output_file_ext) {
      output = replace_file_ext(input_filename, deps_output_file_ext);
    }

    print_deps(output, deps_target ?: replace_file_ext(input_filename, ".o"), do_print_header_deps, do_print_std_deps);

    if (!do_print_deps_and_continue) {
      return 0;
    }
  }

  if (in_c) {
    print_tokens(output_filename, tokens);
    return 0;
  }
  tokens = concat_adjecent_strs(tokens);

  TopLevelObj* codes = parse(tokens);

  gen(output_filename, codes);

  return 0;
}

static int count_input_filenames(Str* inputs) {
  int n = 0;
  for (Str* input = inputs; input; input = input->next) {
    if (start_with_any(input->data, "-l", "-Wl,", NULL)) {
      continue;
    }

    n++;
  }

  return n;
}

static FileType get_file_type(char* fname) {
  if (input_file_type != FILE_NONE) {
    return input_file_type;
  }

  if (end_with(fname, ".so")) {
    return FILE_DSO;
  }

  if (end_with(fname, ".a")) {
    return FILE_AR;
  }

  if (end_with(fname, ".o")) {
    return FILE_OBJ;
  }

  if (end_with(fname, ".c")) {
    return FILE_C;
  }
  if (end_with(fname, ".s")) {
    return FILE_ASM;
  }

  error("unknown file extension: %s", fname);
  return 0;
}

static int run(Str* original) {
  int input_filenames = count_input_filenames(inputs.head.next);

  if (input_filenames <= 0) {
    error("no input files");
  }
  if (input_filenames >= 2 && output_filename && (in_obj || in_asm || in_c)) {
    error("cannot specify '-o' with '-c', '-S' or '-E' with multiple files");
  }

  atexit(unlink_tmp_files);

  StrQueue link_inputs = {};
  for (Str* input = inputs.head.next; input; input = input->next) {
    if (start_with(input->data, "-l")) {
      push_str(&link_inputs, new_str(input->data));
      continue;
    }
    if (start_with(input->data, "-Wl,")) {
      char* opts = strdup(input->data + 4);
      char* opt = strtok(opts, ",");
      while (opt) {
        push_str(&link_args, new_str(opt));
        opt = strtok(NULL, ",");
      }
      continue;
    }

    FileType input_ftype = get_file_type(input->data);

    char* output = output_filename;
    if (!output) {
      char* ext = in_asm ? ".s" : ".o";
      output = replace_file_ext(input->data, ext);
    }

    // .o, .a, .so -> executable
    if (input_ftype == FILE_OBJ || input_ftype == FILE_AR || input_ftype == FILE_DSO) {
      push_str(&link_inputs, new_str(input->data));
      continue;
    }

    if (input_ftype == FILE_ASM) {
      if (in_c || in_asm) {
        continue;
      }

      // .s -> .o
      if (in_obj) {
        int status = run_assembler(input->data, output);
        if (status != 0) {
          return status;
        }
        continue;
      }

      // .s -> executable
      char* tmp_fname = create_tmp_file();
      int status = run_assembler(input->data, tmp_fname);
      if (status != 0) {
        return status;
      }
      push_str(&link_inputs, new_str(tmp_fname));
      continue;
    }

    // .c -> .o
    if (in_obj) {
      int status = drive_to_assemble(original, input->data, output);
      if (status != 0) {
        return status;
      }
      continue;
    }

    // .c -> .s
    if (in_asm) {
      int status = drive_to_compile(original, input->data, output);
      if (status != 0) {
        return status;
      }
      continue;
    }

    // .c -> .c
    if (in_c) {
      int status = preprocesss(original, input->data, output_filename ? output_filename : NULL);
      if (status != 0) {
        return status;
      }
      continue;
    }

    // Keep the input filename to compile, assemble and link later
    // .c -> executable
    push_str(&link_inputs, new_str(input->data));
  }

  return link_inputs.head.next
           ? drive_to_link(original, link_inputs.head.next, output_filename ? output_filename : "a.out")
           : 0;
}

int main(int argc, char** argv) {
  // -D option and -U can define and undefine macros and they take precedence over builtin ones,
  // so it should be before parsing args to define builtin macros.
  define_builtin_macros();

  Str* args = parse_args(argc, argv);
  return !do_exec ? run(args) : exec();
}