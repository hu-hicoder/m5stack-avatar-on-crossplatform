#include <M5GFX.h>
#if defined(SDL_h_)

void setup(void);
void loop(void);
void cleanup(void); // 終了処理関数の宣言

__attribute__((weak)) int user_func(bool *running)
{
  setup();
  do
  {
    loop();
  } while (*running);
  cleanup(); // 終了処理の呼び出し
  return 0;
}

int main(int, char **)
{
  // The second argument is effective for step execution with breakpoints.
  // You can specify the time in milliseconds to perform slow execution that ensures screen updates.
  return lgfx::Panel_sdl::main(user_func, 128);
}

#endif
