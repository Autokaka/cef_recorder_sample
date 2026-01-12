ffmpeg -framerate 30 -i out/frame-%04d.png -c:v libx264 -pix_fmt yuv420p -y out/output.mp4
