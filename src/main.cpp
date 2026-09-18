#include "app/App.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    monitor::App app;
    if (!app.Initialize(instance)) return 0;
    return app.Run();
}
