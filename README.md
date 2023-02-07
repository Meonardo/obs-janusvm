# obs-janusvm
an OBS plugin(output) for publishing OBS media data to janus videoroom.

## Notice
1. The `libwebrtc` is my [fork](https://github.com/Meonardo/libwebrtc/tree/Meonardo) from https://github.com/webrtc-sdk/libwebrtc, 
the dll file is provided in the pre-release [link](https://github.com/Meonardo/obs-janusvm/releases/download/v0.0.3/libwebrtc.dll). 
2. Raw/encoded video(current support NV12 pixel format only).
3. Audio support(convert the raw audio output to `AUDIO_FORMAT_16BIT` sample format).
4. Windows only & only test on 64bit OS.
