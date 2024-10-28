ffmpeg -ss 22    -i  suite_e.mp4 -t 180 -vf "lenscorrection=k1=-0.3:k2=0.1,scale=960:720"  -r 30 -c:v mpeg1video -b:v 1000k -c:a mp2 -b:a 192k video_slices/suite_e_960x720.mpg
ffmpeg -ss 25:21 -i  suite_w.mp4 -t 180 -vf "lenscorrection=k1=-0.3:k2=0.1,scale=960:720"  -r 30 -c:v mpeg1video -b:v 1000k -c:a mp2 -b:a 192k video_slices/suite_w_960x720.mpg
ffmpeg -ss 25:23 -i suite_nw.mp4 -t 180 -vf "lenscorrection=k1=-0.2:k2=0.05,scale=960:540" -r 30 -c:v mpeg1video -b:v 2000k -c:a mp2 -b:a 192k video_slices/suite_nw_960x540.mpg
ffmpeg -ss 25:23 -i suite_se.mp4 -t 180 -vf "lenscorrection=k1=-0.2:k2=0.05,scale=960:540" -r 30 -c:v mpeg1video -b:v 2000k -c:a mp2 -b:a 192k video_slices/suite_se_960x540.mpg

ffmpeg -ss 22    -i  suite_e.mp4 -t 180 -vf "lenscorrection=k1=-0.3:k2=0.1,scale=1280:960"  -r 30 -c:v mpeg1video -b:v 3M -c:a mp2 -b:a 192k video_slices/suite_e_1280x960.mpg
ffmpeg -ss 25:21 -i  suite_w.mp4 -t 180 -vf "lenscorrection=k1=-0.3:k2=0.1,scale=1280:960"  -r 30 -c:v mpeg1video -b:v 3M -c:a mp2 -b:a 192k video_slices/suite_w_1280x960.mpg
ffmpeg -ss 25:23 -i suite_nw.mp4 -t 180 -vf "lenscorrection=k1=-0.2:k2=0.05,scale=1280:720" -r 30 -c:v mpeg1video -b:v 3M -c:a mp2 -b:a 192k video_slices/suite_nw_1280x720.mpg
ffmpeg -ss 25:23 -i suite_se.mp4 -t 180 -vf "lenscorrection=k1=-0.2:k2=0.05,scale=1280:720" -r 30 -c:v mpeg1video -b:v 3M -c:a mp2 -b:a 192k video_slices/suite_se_1280x720.mpg
