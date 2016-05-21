#!/usr/bin/env python
# -*- coding: utf-8 vi:noet
# Merger element for gstreamer

"""
Caps are not negotiated... yet.

"""

import collections, time

import numpy as np
import cv2

import gi

gi.require_version('Gst', '1.0')
gi.require_version('GstBase', '1.0')

from gi.repository import Gst, GObject, GstBase

GObject.threads_init()
Gst.init(None)

class Merger(Gst.Element):
	__gstmetadata__ = (
	 "Merge element demo",
	 "blah",
	 "blah blah",
	 "Jérôme Carretero <cJ@zougloub.eu>"
	)

	def __init__(self):

		src_template = Gst.PadTemplate.new (
		 'src',
		 Gst.PadDirection.SRC,
		 Gst.PadPresence.ALWAYS,
		 Gst.caps_from_string('video/x-raw,format=(string)RGBA'),
		)

		sink_template = Gst.PadTemplate.new (
		 'sink_{l,r}',
		 Gst.PadDirection.SINK,
		 Gst.PadPresence.ALWAYS,
		 Gst.caps_from_string('video/x-raw,format=(string)RGBA'),
		)

		Gst.Element.__init__(self)

		self.sinklpad = Gst.Pad.new_from_template(sink_template, 'sink_l')
		self.sinklpad.set_chain_function_full(self._sinkl_chain, None)
		self.sinklpad.set_event_function_full(self._sinkl_event, None)
		self.add_pad(self.sinklpad)

		self.sinkrpad = Gst.Pad.new_from_template(sink_template, 'sink_r')
		self.sinkrpad.set_chain_function_full(self._sinkr_chain, None)
		self.sinkrpad.set_event_function_full(self._sinkr_event, None)
		self.add_pad(self.sinkrpad)

		self.srcvpad = Gst.Pad.new_from_template(src_template, 'src_v')
		self.srcvpad.set_event_function_full(self._srcv_event, None)
		self.add_pad(self.srcvpad)

		self._bufs_l = collections.deque()
		self._bufs_r = collections.deque()

	def process_video(self, parent):

		while True:

			def popleft_or_none(x):
				try:
					return x.popleft()
				except IndexError:
					return

			Gst.info("Buffers outstanding % 3d / % 3d" % (len(self._bufs_l), len(self._bufs_r)))

			buf_l0 = popleft_or_none(self._bufs_l)
			buf_l1 = popleft_or_none(self._bufs_l)
			buf_r0 = popleft_or_none(self._bufs_r)
			buf_r1 = popleft_or_none(self._bufs_r)

			def app_if(x, y):
				if y is not None:
					x.appendleft(y)

			if buf_l0 is None:
				Gst.info("No L buffers")
				app_if(self._bufs_r, buf_r1)
				app_if(self._bufs_r, buf_r0)
				return

			if buf_r0 is None:
				Gst.info("No R buffers")
				app_if(self._bufs_l, buf_l1)
				app_if(self._bufs_l, buf_l0)
				return

			if buf_l1 is None:
				app_if(self._bufs_l, buf_l0)
				app_if(self._bufs_r, buf_r1)
				app_if(self._bufs_r, buf_r0)
				return

			if buf_r1 is None:
				app_if(self._bufs_r, buf_r0)
				app_if(self._bufs_l, buf_l1)
				app_if(self._bufs_l, buf_l0)
				return

			Gst.debug("Buffer L0 %s ts=%s sz=%s" \
			 % (buf_l0, Gst.TIME_ARGS(buf_l0.pts), buf_l0.get_size()))
			Gst.debug("Buffer L1 %s ts=%s sz=%s" \
			 % (buf_l1, Gst.TIME_ARGS(buf_l1.pts), buf_l1.get_size()))
			Gst.debug("Buffer R0 %s ts=%s sz=%s" \
			 % (buf_r0, Gst.TIME_ARGS(buf_r0.pts), buf_r0.get_size()))
			Gst.debug("Buffer R1 %s ts=%s sz=%s" \
			 % (buf_r1, Gst.TIME_ARGS(buf_r1.pts), buf_r1.get_size()))


			if buf_l0.pts >= buf_r0.pts and buf_l0.pts <= buf_r0.pts + buf_r0.duration:
				Gst.debug("a")
				buf_l = buf_l0
				buf_r = buf_r0
				app_if(self._bufs_l, buf_l1)
				app_if(self._bufs_r, buf_r1)
			elif buf_r0.pts >= buf_l0.pts and buf_r0.pts <= buf_l0.pts + buf_l0.duration:
				Gst.debug("b")
				buf_l = buf_l0
				buf_r = buf_r0
				app_if(self._bufs_l, buf_l1)
				app_if(self._bufs_r, buf_r1)
			elif buf_r0.pts + buf_r0.duration < buf_l0.pts:
				Gst.debug("c")
				app_if(self._bufs_l, buf_l1)
				app_if(self._bufs_l, buf_l0)
				app_if(self._bufs_r, buf_r1)
				Gst.info("Next time for R")
				continue
			elif buf_l0.pts + buf_l0.duration < buf_r0.pts:
				Gst.debug("d")
				app_if(self._bufs_r, buf_r1)
				app_if(self._bufs_r, buf_r0)
				app_if(self._bufs_l, buf_l1)
				Gst.info("Next time for L")
				continue
			else:
				Gst.info("Not implemented!")

			Gst.debug("Pushing new buffer for t=%s" % Gst.TIME_ARGS(buf_l.pts))

			srch, srcw, srcd = self._srch, self._srcw, self._srcd
			dsth, dstw = self._dsth, self._dstw

			tz = list()

			res, mil = buf_l.map(Gst.MapFlags.READ)
			assert res
			res, mir = buf_r.map(Gst.MapFlags.READ)
			assert res

			src0_d = mil.data
			src1_d = mir.data

			img_l = np.fromstring(src0_d, dtype=np.uint8)[:srch*srcw*srcd].reshape((srch, srcw, srcd))
			img_r = np.fromstring(src1_d, dtype=np.uint8)[:srch*srcw*srcd].reshape((srch, srcw, srcd))

			buf_r.unmap(mir)
			buf_l.unmap(mil)

			v = np.hstack((img_l, img_r))

			tz.append(time.time())
			buf_out = Gst.Buffer.new_allocate(None, dsth*dstw*4, None)
			buf_out.dts = buf_l.dts
			buf_out.pts = buf_l.pts
			buf_out.duration = buf_l.duration
			tz.append(time.time())

			buf_out.fill(0, v.tobytes())

			#time.sleep(0.01) # artificial computation time

			#buf_out = Gst.Buffer.new_wrapped(v.data)#, 1280*480*4)

			#res, mio = buf_out.map(Gst.MapFlags.WRITE)
			#print(dir(mio))
			#dst_d = mio.data
			#dst_d[:] = v
			#buf_out.unmap(mio)
			tz.append(time.time())

			tz.append(time.time())
			self.srcvpad.push(buf_out)
			tz.append(time.time())

			Gst.debug("t_alloc %.3f  t_cp %.3f t_push %.3f" \
			 % (tz[1] - tz[0], tz[2] - tz[1], tz[-1] - tz[-2]))
		

	def _sinkl_chain(self, pad, parent, buf_l):
		"""
		Chain function
		"""
		Gst.debug("L Buffer L %s ts=%s sz=%s" \
		 % (buf_l, Gst.TIME_ARGS(buf_l.pts), buf_l.get_size()))

		self._bufs_l.append(buf_l)
		self.process_video(parent)

		res = Gst.FlowReturn.OK if len(self._bufs_l) < 1000 else Gst.FlowReturn.FLUSHING
		return res

	def _sinkr_chain(self, pad, parent, buf_r):
		"""
		Chain function
		"""
		Gst.debug("R Buffer R %s ts=%s sz=%s" \
		 % (buf_r, Gst.TIME_ARGS(buf_r.pts), buf_r.get_size()))

		self._bufs_r.append(buf_r)
		#self.process_video(parent)

		res = Gst.FlowReturn.OK if len(self._bufs_r) < 1000 else Gst.FlowReturn.FLUSHING
		return res

	def setcaps_srcv(self, parent, caps):
		othercaps = self.srcvpad.get_allowed_caps()
		Gst.debug("other caps %s" % othercaps)

		self._srcw = caps[0]["width"]
		self._srch = caps[0]["height"]
		self._srcd = 4

		if 1:
			outcaps = Gst.Caps('%s' % othercaps)

			out_s = outcaps[0]
			Gst.debug("outcasp 0 %s" % (out_s))
			Gst.debug(" %s" % dir(out_s))

			fr = caps[0]["framerate"]
			out_s.set_value("framerate", fr)

			Gst.debug("out_s %s" % out_s.to_string())

			Gst.debug(" dir %s" % dir(outcaps))
			#outcaps.set_structure(0, out_s)
			outcaps = Gst.Caps.from_string(out_s.to_string())


			Gst.debug("pad is %s" % self.srcvpad.__class__)
			Gst.debug("pad can %s" % dir(self.srcvpad))

		self._dstw = outcaps[0]["width"]
		self._dsth = outcaps[0]["height"]

		res = self.srcvpad.push_event(Gst.Event.new_caps(outcaps))

		Gst.info("srcv caps %s" % outcaps)

		self.srcvpad.use_fixed_caps()

		return res

	def _srcv_event(self, pad, parent, event):
		Gst.debug("event %s" % event)
		Gst.debug("event type %s" % event.type)
		if event.type == Gst.EventType.QOS:
			info = event.parse_qos()
			Gst.info("QOS %s" % (str(info)))
		else:
			return self.sinklpad.push_event(event) and self.sinkrpad.push_event(event)

	def _sinkl_event(self, pad, parent, event):
		Gst.debug("event %s" % event)
		Gst.debug("event type %s" % event.type)
		if event.type == Gst.EventType.CAPS:
			caps = event.parse_caps()
			Gst.info("event caps %s" % caps)
			return self.setcaps_srcv(parent, caps)
		return self.srcvpad.push_event(event)

	def _sinkr_event(self, pad, parent, event):
		Gst.debug("event %s" % event)
		Gst.debug("event type %s" % event.type)
		if event.type == Gst.EventType.CAPS:
			caps = event.parse_caps()
			Gst.info("event caps %s" % caps)
			return self.setcaps_srcv(parent, caps)

		return self.srcvpad.push_event(event)


GObject.type_register(Merger)

__gstelementfactory__ = ("merger_py", Gst.Rank.NONE, Merger)
