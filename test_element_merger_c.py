#!/usr/bin/env python
# -*- coding: utf-8 vi:noet

import sys, os, subprocess, shutil

if __name__ == '__main__':

	cmd = [
	 "gst-launch-1.0",
	  "merger_c", "name=merge",
	  "!", "capsfilter", "name=caps_post",
	   "caps=video/x-raw,format=(string)RGBA,width=1280,height=480",
	  "!",
	   #"fakesink",
	   "autovideosink",

	  "videotestsrc", "name=cam_l",
	   "is-live=true",
	   "num-buffers=1000",
	   "!", "queue", "name=q_l",
	   "!", "capsfilter", "name=caps_pre_l",
	    "caps=video/x-raw,format=(string)RGB,width=640,height=480,framerate=30/1",
	   "!", "merge.sink_l",

	  "videotestsrc", "name=cam_r",
	   "is-live=true",
	   "num-buffers=1000",
	   "!", "queue", "name=q_r",
	   "!", "capsfilter", "name=caps_pre_r",
	    "caps=video/x-raw,format=(string)RGB,width=640,height=480,framerate=30/1",
	   "!", "merge.sink_r",
	]

	env = dict()
	env.update(os.environ)
	env["GST_PLUGIN_PATH"] = os.path.join(
	 os.path.dirname(sys.argv[0]),
	 "plugins",
	)

	dotdir = os.path.join(
	 os.path.dirname(sys.argv[0]),
	 "dot",
	)

	env["GST_DEBUG_DUMP_DOT_DIR"] = dotdir

	if os.path.exists(dotdir):
		shutil.rmtree(dotdir)

	os.makedirs(dotdir)

	print(" ".join(cmd))

	res = subprocess.call(cmd,
	 env=env,
	)

	for file in os.listdir(dotdir):
		print("- %s" % file)
		if file.endswith(".dot"):
			path = os.path.join(dotdir, file)
			cmd = ["dot", "-Tpng", path, "-o", path.replace(".dot", ".png")]
			subprocess.call(cmd)

	raise SystemExit(res)
