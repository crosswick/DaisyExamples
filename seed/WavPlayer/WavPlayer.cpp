// # WavPlayer
// ## Description
// Fairly simply sample player.
// Loads 16
//
// Play .wav file from the SD Card.
//
#include <stdio.h>
#include <string.h>
#include "daisy_pod.h"
#include "SdramWavPlayer.h"
//#include "daisy_patch.h"

using namespace daisy;
using local::SdramWavPlayer;

//DaisyPatch   hw;
DaisyPod       hw;
SdmmcHandler   sdcard;
FatFSInterface fsi;
// External SDRAM buffer to avoid clicks per libDaisy issue #535
#define SDRAM_BUF_SAMPS 4096
int16_t DSY_SDRAM_BSS g_sdram_buf[SDRAM_BUF_SAMPS];

SdramWavPlayer sampler;
// Defer file switching out of audio callback
static volatile int g_pending_file = -1; // -1 means no request

void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    int32_t inc;

    // Debounce digital controls
    hw.ProcessDigitalControls();

    // Change file with encoder (request handled in main loop)
    inc = hw.encoder.Increment();
    if(inc != 0)
    {
        size_t curfile = sampler.GetCurrentFile();
        size_t count   = sampler.GetNumberFiles();
        size_t next    = curfile;
        if(inc > 0 && curfile < count - 1)
            next = curfile + 1;
        else if(inc < 0 && curfile > 0)
            next = curfile - 1;
        g_pending_file = static_cast<int>(next);
    }

    //    if(hw.button1.RisingEdge())
    //    {
    //        sampler.Restart();
    //    }
    //
    //    if(hw.button2.RisingEdge())
    //    {
    //        sampler.SetLooping(!sampler.GetLooping());
    //        //hw.SetLed(DaisyPatch::LED_2_B, sampler.GetLooping());
    //        //dsy_gpio_write(&hw.leds[DaisyPatch::LED_2_B],
    //        //               static_cast<uint8_t>(!sampler.GetLooping()));
    //    }

    for(size_t i = 0; i < size; i += 2)
    {
    out[i] = out[i + 1] = s162f(sampler.Stream()) * 0.5f;
    }
}


int main(void)
{
    // Init hardware
    size_t blocksize = 4;
    hw.Init();
    //    hw.ClearLeds();
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sdcard.Init(sd_cfg);
    fsi.Init(FatFSInterface::Config::MEDIA_SD);
    f_mount(&fsi.GetSDFileSystem(), "/", 1);

    sampler.Init(fsi.GetSDPath(), g_sdram_buf, SDRAM_BUF_SAMPS);
    sampler.SetLooping(true);

    // SET LED to indicate Looping status.
    //hw.SetLed(DaisyPatch::LED_2_B, sampler.GetLooping());

    // Init Audio
    hw.SetAudioBlockSize(blocksize);
    hw.StartAudio(AudioCallback);
    // Loop forever...
    for(;;)
    {
        // Handle any pending file switch outside audio thread
        if(g_pending_file >= 0)
        {
            sampler.Open(static_cast<size_t>(g_pending_file));
            g_pending_file = -1;
        }
        // Prepare buffers for sampler as needed
        sampler.Prepare();
    }
}
