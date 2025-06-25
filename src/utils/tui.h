#ifndef TUI_H
#define TUI_H

#include <stddef.h>

// Progress bar configuration
typedef struct {
    size_t current;
    size_t total;
    int width;
    char* label;
    int show_percentage;
    int show_count;
} progress_bar_t;

// Spinner configuration
typedef struct {
    int frame;
    char* label;
    int active;
} spinner_t;

// Initialize progress bar
progress_bar_t* progress_create(const char* label, size_t total, int width);

// Update progress bar
void progress_update(progress_bar_t* bar, size_t current);

// Finish progress bar
void progress_finish(progress_bar_t* bar);

// Free progress bar
void progress_free(progress_bar_t* bar);

// Create spinner
spinner_t* spinner_create(const char* label);

// Update spinner (call periodically)
void spinner_update(spinner_t* spinner);

// Stop spinner
void spinner_stop(spinner_t* spinner);

// Free spinner
void spinner_free(spinner_t* spinner);

// Utility functions
void tui_clear_line(void);
void tui_move_cursor_up(int lines);
void tui_hide_cursor(void);
void tui_show_cursor(void);

// Colored output functions
void tui_success(const char* message);
void tui_error(const char* message);
void tui_warning(const char* message);
void tui_info(const char* message);
void tui_header(const char* message);

#endif // TUI_H