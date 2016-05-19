#!/usr/bin/env python
# -*- coding: utf-8 vi:noet

import sys, os, subprocess

if __name__ == '__main__':

	cmd = [
	 "gst-launch-1.0",
	  "merger", "name=merge",
	  "!", "video/x-raw,format=(string)RGBA,width=640,height=480",
	  "!", "autovideosink",

	  "videotestsrc", "name=cam_l",
	   "!", "queue", "name=q_l",
	   "!", "video/x-raw,format=(string)RGBA,width=640,height=480",
	   "!", "merge.sink_l",

	  "videotestsrc", "name=cam_r",
	   "!", "queue", "name=q_r",
	   "!", "video/x-raw,format=(string)RGBA,width=1280,height=480",
	   "!", "merge.sink_r",
	]

	env = dict()
	env.update(os.environ)
	env["GST_PLUGIN_PATH"] = os.path.join(
	 os.path.dirname(sys.argv[0]),
	 "plugins",
	)

	print(" ".join(cmd))

	res = subprocess.call(cmd,
	 env=env,
	)

	raise SystemExit(res)
