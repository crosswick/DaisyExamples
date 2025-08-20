// Local SDRAM-buffered WAV player based on libDaisy's WavPlayer
#pragma once
#include "daisy_core.h"
#include "ff.h"

namespace local
{
struct WavFileInfo
{
  char name[256];
};

class SdramWavPlayer
{
  public:
    SdramWavPlayer() {}
    ~SdramWavPlayer() {}

    // Provide external SDRAM buffer to avoid internal SRAM contention.
    void Init(const char* search_path, int16_t* ext_buffer, size_t buffer_len);

    int     Open(size_t sel);
    int     Close();
    int16_t Stream();
    void    Prepare();
    void    Restart();
    
  // Seek file to the beginning of the 'data' chunk
  void    SeekToData();

    inline void   SetLooping(bool loop) { looping_ = loop; }
    inline bool   GetLooping() const { return looping_; }
    inline size_t GetNumberFiles() const { return file_cnt_; }
    inline size_t GetCurrentFile() const { return file_sel_; }

  private:
    enum BufferState
    {
        BUFFER_STATE_IDLE,
        BUFFER_STATE_PREPARE_0,
        BUFFER_STATE_PREPARE_1,
    };

    static constexpr size_t kMaxFiles = 8;
    WavFileInfo             file_info_[kMaxFiles];
    size_t                  file_cnt_ = 0, file_sel_ = 0;
    BufferState             buff_state_ = BUFFER_STATE_IDLE;
    int16_t*                buff_       = nullptr;
    size_t                  buff_len_   = 0; // in samples
    size_t                  read_ptr_   = 0;
    bool                    looping_    = false;
    bool                    playing_    = false;
    FIL                     fil_;
};

} // namespace local
