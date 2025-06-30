#define _GNU_SOURCE
#include "tui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "file_utils.h"

// ANSI escape codes
#define ANSI_CLEAR_LINE "\033[2K"
#define ANSI_CURSOR_UP "\033[%dA"
#define ANSI_HIDE_CURSOR "\033[?25l"
#define ANSI_SHOW_CURSOR "\033[?25h"
#define ANSI_BOLD "\033[1m"
#define ANSI_RESET "\033[0m"
#define ANSI_GREEN "\033[32m"
#define ANSI_RED "\033[31m"
#define ANSI_BLUE "\033[34m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_CYAN "\033[36m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_BRIGHT_GREEN "\033[92m"
#define ANSI_BRIGHT_RED "\033[91m"
#define ANSI_BRIGHT_BLUE "\033[94m"

// Spinner frames
static const char* spinner_frames[] = {"|", "/", "-", "\\", "|", "/", "-", "\\"};
static const int spinner_frame_count = 8;

// Get terminal width
static int get_terminal_width(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80; // Default fallback
}

progress_bar_t* progress_create(const char* label, size_t total) {
    progress_bar_t* bar = malloc(sizeof(progress_bar_t));
    if (!bar) return NULL;

    bar->current = 0;
    bar->total = total;
    bar->label = label ? strdup2(label) : NULL;
    bar->show_percentage = 1;
    bar->show_count = 1;

    // Dynamically set width
    int terminal_width = get_terminal_width();
    int label_len = label ? strlen(label) + 2 : 0;
    int percentage_len = 8; // " 100.0%"
    int count_len = 2 + 2 * (int)log10(total > 0 ? total : 1) + 4; // " (total/total)"
    bar->width = terminal_width - label_len - percentage_len - count_len - 5; // 5 for brackets, spaces, etc.
    if (bar->width < 10) bar->width = 10; // Minimum width

    return bar;
}

void progress_update(progress_bar_t* bar, size_t current) {
    if (!bar) return;

    bar->current = current;

    // Calculate progress
    double percentage = bar->total > 0 ? (double)current / bar->total : 0.0;
    int filled = (int)(percentage * bar->width);

    // Clear line and move to beginning
    printf("\r" ANSI_CLEAR_LINE);

    // Print label
    if (bar->label) {
        printf(ANSI_BOLD "%s: " ANSI_RESET, bar->label);
    }

    // Print progress bar
    printf(ANSI_GREEN "[");
    for (int i = 0; i < bar->width; i++) {
        if (i < filled) {
            printf("█");
        } else if (i == filled && percentage < 1.0) {
            printf("▌");
        } else {
            printf(" ");
        }
    }
    printf("]" ANSI_RESET);

    // Print percentage
    if (bar->show_percentage) {
        printf(" %.1f%%", percentage * 100.0);
    }

    // Print count
    if (bar->show_count) {
        printf(" (%zu/%zu)", current, bar->total);
    }

    fflush(stdout);
}

void progress_finish(progress_bar_t* bar) {
    if (!bar) return;

    progress_update(bar, bar->total);
    printf(" " ANSI_GREEN "✓" ANSI_RESET "\n");
}

void progress_free(progress_bar_t* bar) {
    if (!bar) return;
    free(bar->label);
    free(bar);
}

spinner_t* spinner_create(const char* label) {
    spinner_t* spinner = malloc(sizeof(spinner_t));
    if (!spinner) return NULL;

    spinner->frame = 0;
    spinner->label = label ? strdup2(label) : NULL;
    spinner->active = 1;

    tui_hide_cursor();
    return spinner;
}

void spinner_update(spinner_t* spinner) {
    if (!spinner || !spinner->active) return;

    // Clear line and move to beginning
    printf("\r" ANSI_CLEAR_LINE);

    // Print spinner
    printf(ANSI_YELLOW "%s" ANSI_RESET, spinner_frames[spinner->frame]);

    // Print label
    if (spinner->label) {
        printf(" %s", spinner->label);
    }

    // Update frame
    spinner->frame = (spinner->frame + 1) % spinner_frame_count;

    fflush(stdout);
}

void spinner_stop(spinner_t* spinner) {
    if (!spinner) return;

    spinner->active = 0;

    // Clear line and show completion
    printf("\r" ANSI_CLEAR_LINE);
    if (spinner->label) {
        printf(ANSI_GREEN "✓" ANSI_RESET " %s\n", spinner->label);
    }

    tui_show_cursor();
}

void spinner_free(spinner_t* spinner) {
    if (!spinner) return;
    free(spinner->label);
    free(spinner);
}

void spinner_set_label(spinner_t* spinner, const char* label) {
    if (!spinner) return;
    free(spinner->label);
    spinner->label = label ? strdup2(label) : NULL;
}

void tui_clear_line(void) {
    printf(ANSI_CLEAR_LINE "\r");
    fflush(stdout);
}

void tui_move_cursor_up(int lines) {
    printf(ANSI_CURSOR_UP, lines);
    fflush(stdout);
}

void tui_hide_cursor(void) {
    printf(ANSI_HIDE_CURSOR);
    fflush(stdout);
}

void tui_show_cursor(void) {
    printf(ANSI_SHOW_CURSOR);
    fflush(stdout);
}

// Colored output functions
void tui_success(const char* message) {
    printf(ANSI_BRIGHT_GREEN "✓" ANSI_RESET " %s\n", message);
}

void tui_error(const char* message) {
    printf(ANSI_BRIGHT_RED "✗" ANSI_RESET " %s\n", message);
}

void tui_warning(const char* message) {
    printf(ANSI_YELLOW "⚠" ANSI_RESET " %s\n", message);
}

void tui_info(const char* message) {
    printf(ANSI_BRIGHT_BLUE "ℹ" ANSI_RESET " %s\n", message);
}

void tui_header(const char* message) {
    printf(ANSI_BOLD ANSI_CYAN "=== %s ===" ANSI_RESET "\n", message);
}
