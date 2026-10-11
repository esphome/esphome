import esphome.codegen as cg

AUTO_LOAD = ["camera"]
CODEOWNERS = ["@jvgelder"]

camera_video_ns = cg.esphome_ns.namespace("camera_video")
CameraVideoFrame = camera_video_ns.class_("CameraVideoFrame")
CameraVideoSourceListener = camera_video_ns.class_("CameraVideoSourceListener")
CameraVideoSource = camera_video_ns.class_("CameraVideoSource")
VideoPixelFormat = camera_video_ns.enum("VideoPixelFormat", is_class=True)
H264Frame = camera_video_ns.class_("H264Frame")
H264StreamListener = camera_video_ns.class_("H264StreamListener")
H264Stream = camera_video_ns.class_("H264Stream")
