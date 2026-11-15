# GIF assets

Put robot animations here, named exactly:

```
robot_idle.gif  robot_blink.gif  robot_happy.gif  robot_sleepy.gif  robot_surprised.gif
robot_look_left.gif  robot_look_right.gif  robot_weather_sunny.gif  robot_weather_rain.gif
```

Any subset works. Missing or corrupt files fall back to the built-in drawn face.
Use `python tools/convert_gif.py <file> --name <slot>` to resize, cap the frame rate and
reduce colours. Only add GIFs you have the right to redistribute. None are bundled.
