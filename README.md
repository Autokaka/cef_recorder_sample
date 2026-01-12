cef_recorder_sample

create sample server

`python3 -m http.server --bind 0.0.0.0 8000`

run sample recorder

1. `cmake -S . -B build`
2. `rm -rf out && mkdir -p out && ./build/bin/pup_cef_sample`
3. `sh gen_video.sh`
