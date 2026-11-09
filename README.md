# avcpp-test-data

Binary fixtures for the [avcpp](https://github.com/h4tr3d/avcpp) codec
send/receive tests.

The fixtures are intentionally small (a few hundred kilobytes in total) and are
checked into Git so a test run does not depend on FFmpeg encoders or on the
network.

## Layout

```
media/          generated media files
manifest.json   sizes, SHA-256 digests and stream statistics
generate.sh     regenerates media and manifest
```

`manifest.json` describes every fixture:

```json
{
  "version": 1,
  "media": [
    {
      "file": "media/h264-baseline-320x240-25fps.mp4",
      "size": 59469,
      "sha256": "2959d5bc3a9dcc7d77608e487bd35dfdb17253f59c9023743212c19e6557f82e",
      "streams": [
        {
          "index": 0,
          "type": "video",
          "codec": "h264",
          "width": 320,
          "height": 240,
          "packets": 50,
          "frames": 50
        }
      ]
    }
  ]
}
```

`packets` and `frames` are the values reported by `ffprobe -count_packets
-count_frames`. They depend on the FFmpeg build, so the avcpp tests assert them
only where demuxers and decoders are stable across releases (video), and assert
decoded sample totals for audio.

## Fixtures

| File | Purpose |
| --- | --- |
| `h264-baseline-320x240-25fps.mp4` | 2 s, 50 frames, no B-frames, one packet per frame |
| `h264-bframes-320x240-25fps.mp4` | 2 s, 50 frames, B-frames, exercises reordering and decoder delay |
| `aac-tone-48khz-stereo.m4a` | 2 s tone; 95 packets, 94 decoded frames, exercises decoder priming |
| `pcm-s16le-tone-48khz-stereo.wav` | 2 s tone; 24 packets, 96000 samples, no decoder delay |

## Using the fixtures

CMake:

```cmake
FetchContent_Declare(avcpp_test_data
    GIT_REPOSITORY https://github.com/h4tr3d/avcpp-test-data.git
    GIT_TAG master
)
FetchContent_MakeAvailable(avcpp_test_data)

target_link_libraries(my_tests PRIVATE avcpp::test_data)
```

The interface target defines `AVCPP_TEST_DATA_DIR` as a string literal with the
absolute path to the `media` directory. For reproducible builds, replace
`master` with a commit hash.

Meson:

```meson
avcpp_test_data = subproject('avcpp-test-data')
my_tests = executable('my_tests', 'main.cpp',
    dependencies: avcpp_test_data.get_variable('avcpp_test_data_dep'))
```

## Regenerating

```sh
./generate.sh
```

The script uses the FFmpeg CLI and Python 3. Regenerated files are only
byte-identical when the same FFmpeg build is used; if the bytes change, commit
the new media and `manifest.json` together.

## License

The fixtures are released under CC0 1.0 Universal. See `LICENSE`.
