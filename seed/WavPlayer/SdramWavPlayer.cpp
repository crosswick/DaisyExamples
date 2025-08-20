#include <cstring>
#include "SdramWavPlayer.h"

using namespace local;

void SdramWavPlayer::Init(const char* search_path, int16_t* ext_buffer, size_t buffer_len)
{
    buff_     = ext_buffer;
    buff_len_ = buffer_len; // in samples
    file_sel_ = 0;
    file_cnt_ = 0;
    playing_  = true;
    looping_  = false;

    // Scan directory for WAV files
    FRESULT result = FR_OK;
    FILINFO fno;
    DIR     dir;
    char*   fn;
    if(f_opendir(&dir, search_path) != FR_OK)
    {
        return;
    }
    do
    {
        result = f_readdir(&dir, &fno);
        if(result != FR_OK || fno.fname[0] == 0)
            break;
        if(fno.fattrib & (AM_HID | AM_DIR))
            continue;
        fn = fno.fname;
        if(file_cnt_ < (kMaxFiles - 1))
        {
            if(strstr(fn, ".wav") || strstr(fn, ".WAV"))
            {
                strcpy(file_info_[file_cnt_].name, search_path);
                strcat(file_info_[file_cnt_].name, fn);
                file_cnt_++;
            }
        }
        else
        {
            break;
        }
    } while(result == FR_OK);
    f_closedir(&dir);

    // No need to pre-read WAV headers; we'll seek to 'data' per file on open.

    // Open first file and prefill both halves of the SDRAM buffer
    Open(0); // Open() seeks to start of data and primes both halves

    // Ready to start reading from the beginning (Open() already set these)
    buff_state_ = BUFFER_STATE_IDLE;
    read_ptr_   = 0;
}

int SdramWavPlayer::Open(size_t sel)
{
    if(sel != file_sel_)
    {
        f_close(&fil_);
        file_sel_ = sel < file_cnt_ ? sel : file_cnt_ - 1;
    }
    FRESULT fr = f_open(&fil_, file_info_[file_sel_].name, (FA_OPEN_EXISTING | FA_READ));
    if(fr == FR_OK)
    {
    // Seek and prefill both halves so playback switches cleanly
    SeekToData();
    read_ptr_   = 0;
    playing_    = true;
    // Fill first half
    buff_state_ = BUFFER_STATE_PREPARE_0;
    Prepare();
    // Fill second half
    buff_state_ = BUFFER_STATE_PREPARE_1;
    Prepare();
    buff_state_ = BUFFER_STATE_IDLE;
    }
    return fr;
}

int SdramWavPlayer::Close()
{
    return f_close(&fil_);
}

int16_t SdramWavPlayer::Stream()
{
    int16_t samp;
    if(playing_ && buff_ && buff_len_)
    {
        samp = buff_[read_ptr_];
        read_ptr_ = (read_ptr_ + 1) % buff_len_;
        if(read_ptr_ == 0)
            buff_state_ = BUFFER_STATE_PREPARE_1;
        else if(read_ptr_ == buff_len_ / 2)
            buff_state_ = BUFFER_STATE_PREPARE_0;
    }
    else
    {
        samp = 0;
        if(looping_)
            playing_ = true;
    }
    return samp;
}

void SdramWavPlayer::Prepare()
{
    if(buff_state_ == BUFFER_STATE_IDLE || !buff_ || buff_len_ == 0)
        return;

    size_t offset    = (buff_state_ == BUFFER_STATE_PREPARE_1) ? (buff_len_ / 2) : 0;
    size_t rx_samps  = buff_len_ / 2;
    size_t rx_bytes  = rx_samps * sizeof(buff_[0]);
    size_t bytesread = 0;

    f_read(&fil_, &buff_[offset], rx_bytes, &bytesread);
    if(bytesread < rx_bytes || f_eof(&fil_))
    {
        if(looping_)
        {
            Restart();
            size_t samp_advance = bytesread / sizeof(buff_[0]);
            f_read(&fil_, &buff_[offset + samp_advance], rx_bytes - bytesread, &bytesread);
        }
        else
        {
            playing_ = false;
        }
    }
    buff_state_ = BUFFER_STATE_IDLE;
}

void SdramWavPlayer::Restart()
{
    playing_ = true;
    SeekToData();
}

// Removed unused GetNextBuffState()

// Helper to scan RIFF/WAVE and locate start of 'data' chunk
void SdramWavPlayer::SeekToData()
{
    // Basic structures for RIFF parsing
    struct __attribute__((packed)) RiffHeader
    {
        char     ChunkID[4];   // "RIFF"
        uint32_t ChunkSize;
        char     Format[4];    // "WAVE"
    };
    struct __attribute__((packed)) ChunkHeader
    {
        char     ID[4];
        uint32_t Size;
    };

    // Seek to start and read RIFF
    f_lseek(&fil_, 0);
    RiffHeader riff;
    UINT       br = 0;
    if(f_read(&fil_, &riff, sizeof(riff), &br) != FR_OK || br != sizeof(riff))
        return;
    // Iterate chunks until 'data'
    for(;;)
    {
        ChunkHeader ch{};
        if(f_read(&fil_, &ch, sizeof(ch), &br) != FR_OK || br != sizeof(ch))
            return;
        if(memcmp(ch.ID, "data", 4) == 0)
        {
            // We're at the start of data payload
            return; // file position is right at data start
        }
        // Skip this chunk (account for odd size padding)
        DWORD skip = ch.Size;
        f_lseek(&fil_, f_tell(&fil_) + skip + (skip & 1));
    }
}
