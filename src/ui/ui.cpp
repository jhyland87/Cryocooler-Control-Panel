#include "ui.h"
#include "dashboard.h"

extern "C" void ui_init(void) {
    dashboard_init();
}

extern "C" void ui_tick(void) {
    dashboard_tick();
}
