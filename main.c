#include "cc.h"

char* input_filename;
char* output_filename;
bool do_log_args;
bool do_exec;

static void usage(int status) {
  fprintf(stderr, "Usage: cc <filename>\n");
  exit(status);
}

static bool are_equal(char* a, char* b) {
  return strcmp(a, b) == 0;
}

static bool are_n_equal(char* a, char* b) {
  return strncmp(a, b, strlen(b)) == 0;
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

    if (are_equal(argv[i], "-o")) {
      if (!argv[++i]) {
        usage(1);
      }
      output_filename = argv[i];
      continue;
    }

    if (are_n_equal(argv[i], "-o")) {
      output_filename = argv[i] + 2;
      continue;
    }

    if (argv[i][0] == '-' && argv[i][1] != '\0') {
      error("unknown argument: %s", argv[i]);
    }

    input_filename = argv[i];
  }

  if (!input_filename) {
    error("no input files");
  }
}

int run_subprocess(int argc, char** argv) {
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

int run_to_exec(int argc, char** argv) {
  char** vals = calloc(argc + 1 + 1, sizeof(char*));  // the last +1 for NULL termination
  memcpy(vals, argv, argc * sizeof(char*));
  vals[argc++] = "-exec";
  return run_subprocess(argc, vals);
}

int exec() {
  Token* tokens = tokenize();
  TopLevelObj* codes = parse(tokens);
  gen(codes);
  return 0;
}

int main(int argc, char** argv) {
  parse_args(argc, argv);
  return !do_exec ? run_to_exec(argc, argv) : exec();
}