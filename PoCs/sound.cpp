#include <SDL2/SDL.h>

#include <thread>
#include <chrono>
#include <iostream>

constexpr SDL_AudioSpec idealAudioFormat {
	    .freq = 44100,
	    .format = 0x8010, //signed int 16
	    .channels = 2,
	    .samples = 44100,
	    .callback = NULL,//TODO investigate callback for preventing audio loops during potential long frame time
	    .userdata = NULL,
	  };

#define PI (3.1415926535897932384626f)

static auto& operator<<(decltype(std::cout)& co, const SDL_AudioSpec& as) {
  std::cout << "freq: " << std::dec << as.freq << "\n";
  std::cout << "format: 0x" << std::hex << as.format << "\n";
  std::cout << "channels: " << std::dec << (int)as.channels << "\n";
  std::cout << "silence: " << (int)as.silence << "\n";
  std::cout << "samples: " << (int)as.samples << "\n";
  std::cout << "size: " << as.size << "\n";
  return co;
}

int main(int argc, char** argv) {
  SDL_InitSubSystem(SDL_INIT_AUDIO);
  SDL_AudioSpec audioFormat;//set below to match real device
  SDL_AudioDeviceID adev = SDL_OpenAudioDevice(NULL, 0, &idealAudioFormat, &audioFormat, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
  if(adev == 0) {
    std::cout << "failed to open audio device: " << SDL_GetError() << "\n";
    return 1;
  }
  std::cout << "captured format:\n" << audioFormat;
  float scale = 440*2*PI/audioFormat.freq;//scale input for 440hz
  const float volume = 0.3f;
  const int stride = audioFormat.channels*SDL_AUDIO_BITSIZE(audioFormat.format)/8;
  const uint32_t dataLen = stride * audioFormat.freq;
  int16_t bits;
  int16_t *data = (int16_t*)malloc(dataLen);//one second of data
  SDL_PauseAudioDevice(adev, 0);
  for(int i = 0;i < audioFormat.freq;i++) {
    float value = sin(i*scale) * volume;
    bits = static_cast<int64_t>(value*0x7fff);
    for(int c = 0;c < audioFormat.channels;c++)
      data[audioFormat.channels*i+c] = bits;
  }
  if(SDL_QueueAudio(adev, reinterpret_cast<void*>(data), dataLen) != 0)
    std::cout << "error queuing audio: " << SDL_GetError() << "\n";
  free(data);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  SDL_PauseAudioDevice(adev, 1);
  SDL_CloseAudioDevice(adev);
}

