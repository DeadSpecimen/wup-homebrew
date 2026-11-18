/* The installer's screens on the host: its real drawing code (main.cpp, Gfx.cpp) with the Wii U's calls stubbed
 * (include/), each state saved as a PNG. test/preview/run.sh */
#include <SDL.h>
#include "ProcUI.hpp"
#include "drc.hpp"
namespace ProcUI { void Init() {} void Shutdown() {} bool IsRunning() { return false; } void StopRunning() {} void SetHomeButtonMenuEnabled(bool) {} }
namespace Drc { bool Init(std::string&) { return true; } void Shutdown() {} bool FirmwarePath(std::string&) { return true; }
                bool Start(const std::string&, std::string&) { return true; } bool Poll(int&) { return true; } bool Finish(std::string&) { return true; } }
#define main installer_main
#include "../../source/main.cpp"
#undef main
#include <SDL_image.h>

/* Gfx.cpp is built with SDL_RenderPresent -> preview_present: the frame is read before it is shown */
static SDL_Surface* frame;
extern "C" void preview_present(SDL_Renderer* r)
{
    int w, h;
    SDL_GetRendererOutputSize(r, &w, &h);
    if (frame) SDL_FreeSurface(frame);
    frame = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(r, nullptr, SDL_PIXELFORMAT_ARGB8888, frame->pixels, frame->pitch);
    SDL_RenderPresent(r);
}
static void Shot(const char* name)
{
    Draw();
    std::string path = std::string(PREVIEW_OUT) + "/" + name + ".png";
    IMG_SavePNG(frame, path.c_str());
    printf("%s\n", path.c_str());
}

int main()
{
    Gfx::Init();
    IMG_Init(IMG_INIT_PNG);
    state = LOADING; shownAt = 0; Shot("0a-disclaimer-waiting");
    shownAt = SDL_GetTicks() - DISCLAIMER_MS - 2600; Shot("0b-disclaimer-press-a");     // (at full brightness)
    if (getenv("PREVIEW_PULSE"))                                       // the prompt's first 3 s, 10 frames a second
        for (int ms = 0; ms <= 3000; ms += 100) {
            shownAt = SDL_GetTicks() - DISCLAIMER_MS - ms;
            char n[32]; snprintf(n, sizeof n, "pulse/%04d", ms); Shot(n);
        }
    if (getenv("PREVIEW_FADE")) {                                      // the disclaimer in from black
        for (int ms = 0; ms <= (int)FADE_MS; ms += 50) {
            shownAt = SDL_GetTicks() - ms; char n[32]; snprintf(n, sizeof n, "fadein/%04d", ms); Shot(n);
        }
    }
    if (getenv("PREVIEW_FADE")) {                                      // the disclaimer out to black, the installer in
        shownAt = SDL_GetTicks() - DISCLAIMER_MS - 2600;
        for (int ms = 0; ms <= (int)FADE_MS; ms += 50) {
            fadeOutAt = SDL_GetTicks() - ms; char n[32]; snprintf(n, sizeof n, "fade/a%04d", ms); Shot(n);
        }
        state = MENU; fadeOutAt = 0;
        for (int ms = 0; ms <= (int)FADE_MS; ms += 50) {
            fadeInAt = SDL_GetTicks() - ms; char n[32]; snprintf(n, sizeof n, "fade/b%04d", ms); Shot(n);
        }
        fadeInAt = 0;
    }
    state = MENU; Shot("1-menu");
    row = 1; Shot("1b-menu-remove"); row = 0;
    cantInstall = "Unsupported GamePad firmware version"; Shot("1c-menu-unsupported"); cantInstall.clear();
    installing = true; state = CONFIRM; Shot("2-confirm-install");
    state = FLASHING; progress = 42; Shot("3-installing");
    state = DONE; Shot("4-installed");
    state = FAILED; failure = "The GamePad is not connected"; Shot("5-failed");
    installing = false; state = CONFIRM; Shot("6-confirm-remove");
    state = DONE; Shot("7-removed");
    state = BLOCKED; blocked = "Needs Aroma or Tiramisu"; Shot("8-blocked");
    Gfx::Shutdown();
    return 0;
}
