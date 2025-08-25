#include "led-matrix.h"

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include <exception>
#include <Magick++.h>
#include <magick/image.h>

using rgb_matrix::Canvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::FrameCanvas;

volatile bool interrupt_received = false;
static void InterruptHandler(int signo) {
  interrupt_received = true;
}

using ImageVector = std::vector<Magick::Image>;

static ImageVector LoadImageAndScaleImage(const char *filename,
                                          int target_width,
                                          int target_height) {
  ImageVector result;

  ImageVector frames;
  try {
    readImages(&frames, filename);
  } catch (std::exception &e) {
    if (e.what())
      fprintf(stderr, "%s\n", e.what());
    return result;
  }

  if (frames.empty()) {
    fprintf(stderr, "No image found.");
    return result;
  }

  // Animated images have partial frames that need to be put together
  if (frames.size() > 1) {
    Magick::coalesceImages(&result, frames.begin(), frames.end());
  } else {
    result.push_back(frames[0]); // just a single still image.
  }

  for (Magick::Image &image : result) {
    image.scale(Magick::Geometry(target_width, target_height));
  }

  return result;
}

void CopyImageToCanvas(const Magick::Image &image, Canvas *canvas) {
  const int offset_x = 0, offset_y = -33;  // If you want to move the image.
  // Copy all the pixels to the canvas.
  for (size_t y = 0; y < image.rows(); ++y) {
    for (size_t x = 0; x < image.columns(); ++x) {
      const Magick::Color &c = image.pixelColor(x, y);
      if (c.alphaQuantum() < 256) {
        canvas->SetPixel(x + offset_x, y + offset_y,
                         ScaleQuantumToChar(c.redQuantum()),
                         ScaleQuantumToChar(c.greenQuantum()),
                         ScaleQuantumToChar(c.blueQuantum()));
      }
    }
  }
}

void CopyImageToCanvas(const Magick::Image &image, Canvas *canvas, int shownImageX, int shownImageY, bool *check) {// Copy all the pixels to the canvas.
  int offset_x = shownImageX * 128;
  int offset_y = shownImageY * 32;  // If you want to move the image.

  if(*check == false){
    if (((image.columns() % 128) == 0) && ((image.rows() % 32) == 0)){
      *check = true;
    }
  }
  else {
    for (size_t y = 0; y < image.rows(); ++y) {
      for (size_t x = 0; x < image.columns(); ++x) {
        const Magick::Color &c = image.pixelColor(x, y);
        if (c.alphaQuantum() < 256) {
          canvas->SetPixel(x + offset_x, y + offset_y, ScaleQuantumToChar(c.redQuantum()), ScaleQuantumToChar(c.greenQuantum()), ScaleQuantumToChar(c.blueQuantum()));
        }
      }
    }
  }
}

void ShowAnimatedImage(const ImageVector &images, RGBMatrix *matrix) {
  FrameCanvas *offscreen_canvas = matrix->CreateFrameCanvas();
  while (!interrupt_received) {
    for (const auto &image : images) {
      if (interrupt_received) break;
      CopyImageToCanvas(image, offscreen_canvas);
      offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
      usleep(image.animationDelay() * 10000);  // 1/100s converted to usec
    }
  }
}

int main(int argc, char *argv[]){

  Magick::InitializeMagick(*argv);

  RGBMatrix::Options my_defaults;
  my_defaults.hardware_mapping = "adafruit-hat-pwm";
  my_defaults.led_rgb_sequence = "GBR";
  //my_defaults.disable_busy_waiting = true;
  my_defaults.row_address_type = 0;
  my_defaults.pwm_lsb_nanoseconds = 130;
  my_defaults.rows = 32;
  my_defaults.cols = 64;
  my_defaults.chain_length = 2;
  my_defaults.brightness = 80;
  my_defaults.show_refresh_rate = false;

  rgb_matrix::RuntimeOptions runtime_defaults;

  runtime_defaults.drop_privileges = 1;

  RGBMatrix *matrix = RGBMatrix::CreateFromFlags(&argc, &argv, &my_defaults, &runtime_defaults);

  if (matrix == NULL){
    PrintMatrixFlags(stderr, my_defaults, runtime_defaults);
    return 1;
  }

  char *filename = "FaceShiftTest.png";

  signal(SIGTERM, InterruptHandler);
  signal(SIGINT, InterruptHandler);

  ImageVector images = LoadImageAndScaleImage(filename, 128, 131/*, matrix->width(), matrix->height()*/);

  int xCOR = 0;
  int yCOR = 0;
  bool ImageCheck = true;

  switch (images.size()){
    case 0:
      break;
    case 1:
      while (!interrupt_received){
        CopyImageToCanvas(images[0], matrix, xCOR, yCOR, &ImageCheck);
        yCOR++;
        printf("%d\n", yCOR);
        if (yCOR == 4){
          yCOR = 0;
        }
        usleep(500000);
      }
      break;
    default:
      ShowAnimatedImage(images, matrix);
      break;
  }

  matrix->Clear();
  delete matrix;

  return 0;

}
