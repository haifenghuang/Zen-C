#include "cmd.h"
#include "colors.h"
#include "../zprep.h"
#include "../codegen/compat.h"
#include "../zen/zen_facts.h"
#include "../platform/os.h"
#include "../parser/parser.h"

void print_search_paths()
{
    char exe_path[8192] = {0};
    z_get_executable_path(exe_path, sizeof(exe_path));

    printf("Search paths:\n");
    printf("  ./\n");
    printf("  %s/std\n", exe_path);
    printf("  %s/../share/zenc/std\n", exe_path);
}

void print_version()
{
    printf(COLOR_BOLD "zc" COLOR_RESET " %s\n", ZEN_VERSION);
}

void print_usage()
{
    printf(COLOR_BOLD "Zen C" COLOR_RESET " - The language of monks\n\n");
    printf(COLOR_BOLD "Usage:" COLOR_RESET
                      " zc [command] [options] <file.zc> [extra files...]\n\n");
    printf(COLOR_BOLD COLOR_YELLOW "Commands:" COLOR_RESET "\n");
    printf("  " COLOR_GREEN "run" COLOR_RESET "          Compile and run the program\n");
    printf("  " COLOR_GREEN "build" COLOR_RESET "        Compile to executable\n");
    printf("  " COLOR_GREEN "check" COLOR_RESET "        Check for errors only\n");
    printf("  " COLOR_GREEN "repl" COLOR_RESET "         Start Interactive REPL\n");
    printf("  " COLOR_GREEN "transpile" COLOR_RESET
           "    Transpile to C code only (no compilation)\n");
    printf("  " COLOR_GREEN "lsp" COLOR_RESET "          Start Language Server\n");
    printf("\n" COLOR_BOLD COLOR_YELLOW "Options:" COLOR_RESET "\n");
    printf("  " COLOR_CYAN "-o" COLOR_RESET " <file>       Output executable name\n");
    printf("  " COLOR_CYAN "-O" COLOR_RESET "<level>       Optimization level\n");
    printf("  " COLOR_CYAN "-g" COLOR_RESET "              Debug info\n");
    printf("  " COLOR_CYAN "-c" COLOR_RESET "              Compile only (produce .o)\n");
    printf("  " COLOR_CYAN "-v" COLOR_RESET ", " COLOR_CYAN "--verbose" COLOR_RESET
           "   Verbose output\n");
    printf("  " COLOR_CYAN "-q" COLOR_RESET ", " COLOR_CYAN "--quiet" COLOR_RESET
           "     Quiet output\n");
    printf("  " COLOR_CYAN "--emit-c" COLOR_RESET "        Keep generated C file (out.c)\n");
    printf("  " COLOR_CYAN "--keep-comments" COLOR_RESET " Preserve comments in output C\n");
    printf("  " COLOR_CYAN "--freestanding" COLOR_RESET "  Freestanding mode (no stdlib)\n");
    printf("  " COLOR_CYAN "--cc" COLOR_RESET
           " <compiler> C compiler to use (gcc, clang, tcc, zig)\n");
    printf("  " COLOR_CYAN "--typecheck" COLOR_RESET "     Enable semantic analysis\n");
    printf("  " COLOR_CYAN "--json" COLOR_RESET "          Emit diagnostics as JSON\n");
    printf("  " COLOR_CYAN "--no-zen" COLOR_RESET "        Disable Zen facts\n");
    printf("  " COLOR_CYAN "--cpp" COLOR_RESET "           Use C++ mode\n");
    printf("  " COLOR_CYAN "--objective-c" COLOR_RESET "   Use Objective-C mode\n");
    printf("  " COLOR_CYAN "--cuda" COLOR_RESET "          Use CUDA mode (requires nvcc)\n");
    printf("  " COLOR_CYAN "--help" COLOR_RESET "          Print this help message\n");
    printf("  " COLOR_CYAN "--paths" COLOR_RESET "         Print library search paths\n");
    printf("  " COLOR_CYAN "--version" COLOR_RESET "       Print version information\n");
}

void build_compile_command(char *cmd, size_t cmd_size, const char *outfile,
                           const char *temp_source_file, const char *extra_c_sources)
{
    CmdBuilder cb;
    cmd_init(&cb);

    // Compiler
    cmd_add(&cb, g_config.cc);

    // GCC Flags
    cmd_add(&cb, g_config.gcc_flags);
    cmd_add(&cb, g_cflags);

    // Freestanding
    if (g_config.is_freestanding)
    {
        cmd_add(&cb, "-ffreestanding");
    }

    // Quiet
    if (g_config.quiet)
    {
        cmd_add(&cb, "-w");
    }

    // Output file
    cmd_add(&cb, "-o");
    cmd_add(&cb, outfile);

    // Input files
    cmd_add(&cb, temp_source_file);
    cmd_add(&cb, extra_c_sources);

    // Platform flags
    if (!z_is_windows() && !g_config.is_freestanding)
    {
        cmd_add(&cb, "-lm");
        if (g_parser_ctx && g_parser_ctx->has_async)
        {
            cmd_add(&cb, "-lpthread");
        }
    }

    // Linker flags
    cmd_add(&cb, g_link_flags);
    if (z_is_windows())
    {
        cmd_add(&cb, "-lws2_32");
    }

    // Include paths
    char exe_path[8192] = {0};
    z_get_executable_path(exe_path, sizeof(exe_path));

    char dev_std[9000];
    snprintf(dev_std, sizeof(dev_std), "%s/std", exe_path);

    if (access(dev_std, F_OK) == 0)
    {
        cmd_add_fmt(&cb, "-I\"%s\"", exe_path);
        if (!g_config.is_freestanding)
        {
            cmd_add_fmt(&cb, "-I\"%s/std/third-party/tre/include\"", exe_path);
        }
    }
    else
    {
        char install_std[9000];
        snprintf(install_std, sizeof(install_std), "%s/../share/zenc/std", exe_path);

        if (access(install_std, F_OK) == 0)
        {
            cmd_add_fmt(&cb, "-I\"%s/../share/zenc\"", exe_path);
            if (!g_config.is_freestanding)
            {
                cmd_add_fmt(&cb, "-I\"%s/../share/zenc/std/third-party/tre/include\"", exe_path);
            }
        }
        else
        {
            cmd_add(&cb, "-I.");
            if (!g_config.is_freestanding)
            {
                cmd_add(&cb, "-I./std/third-party/tre/include");
            }
        }
    }

    // Copy to output buffer
    if (cb.len < cmd_size)
    {
        strcpy(cmd, cb.buf);
    }
    else
    {
        // Truncate if necessary (though we should avoid this)
        strncpy(cmd, cb.buf, cmd_size - 1);
        cmd[cmd_size - 1] = 0;
        zwarn("Command buffer truncated!");
    }

    cmd_free(&cb);
}

void cmd_init(CmdBuilder *cmd)
{
    cmd->cap = 1024;
    cmd->len = 0;
    cmd->buf = xmalloc(cmd->cap);
    cmd->buf[0] = '\0';
}

static void ensure_cap(CmdBuilder *cmd, size_t needed)
{
    if (cmd->len + needed >= cmd->cap)
    {
        while (cmd->len + needed >= cmd->cap)
        {
            cmd->cap *= 2;
        }
        cmd->buf = xrealloc(cmd->buf, cmd->cap);
    }
}

void cmd_add(CmdBuilder *cmd, const char *str)
{
    if (!str || !str[0])
    {
        return;
    }

    size_t len = strlen(str);
    size_t needed = len + 1 + 1; // + space + null terminator

    ensure_cap(cmd, needed);

    if (cmd->len > 0 && cmd->buf[cmd->len - 1] != ' ')
    {
        strcat(cmd->buf, " ");
        cmd->len++;
    }

    strcat(cmd->buf, str);
    cmd->len += len;
}

void cmd_add_fmt(CmdBuilder *cmd, const char *fmt, ...)
{
    va_list args;

    // First pass to get size
    va_start(args, fmt);
    int size = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (size < 0)
    {
        return;
    }

    size_t needed = size + 1 + 1; // + space + null
    ensure_cap(cmd, needed);

    if (cmd->len > 0 && cmd->buf[cmd->len - 1] != ' ')
    {
        strcat(cmd->buf, " ");
        cmd->len++;
    }

    va_start(args, fmt);
    vsnprintf(cmd->buf + cmd->len, cmd->cap - cmd->len, fmt, args);
    va_end(args);

    cmd->len += size;
}

void cmd_free(CmdBuilder *cmd)
{
    free(cmd->buf);
    cmd->buf = NULL;
    cmd->len = 0;
    cmd->cap = 0;
}

const char *cmd_to_string(CmdBuilder *cmd)
{
    return cmd->buf;
}
