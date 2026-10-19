// camsnap <camera name substring> <output.jpg> [frames to skip]
// Grabs one frame from an AVFoundation camera after letting exposure settle.
import AVFoundation
import AppKit
import CoreImage

let args = CommandLine.arguments
guard args.count >= 3 else {
  FileHandle.standardError.write("usage: camsnap <camera name> <out.jpg> [skip]\n".data(using: .utf8)!)
  exit(2)
}
let wanted = args[1].lowercased()
let outPath = args[2]
let skip = args.count > 3 ? Int(args[3]) ?? 30 : 30

let discovery = AVCaptureDevice.DiscoverySession(
  deviceTypes: [.external, .builtInWideAngleCamera], mediaType: .video, position: .unspecified)
guard let device = discovery.devices.first(where: { $0.localizedName.lowercased().contains(wanted) }) else {
  let names = discovery.devices.map { $0.localizedName }.joined(separator: ", ")
  FileHandle.standardError.write("no camera matching '\(wanted)'; found: \(names)\n".data(using: .utf8)!)
  exit(1)
}

final class Grabber: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate {
  var seen = 0
  let skip: Int
  let out: String
  init(skip: Int, out: String) { self.skip = skip; self.out = out }
  func captureOutput(_ output: AVCaptureOutput, didOutput sampleBuffer: CMSampleBuffer,
                     from connection: AVCaptureConnection) {
    seen += 1
    guard seen > skip, let pixels = CMSampleBufferGetImageBuffer(sampleBuffer) else { return }
    let image = CIImage(cvPixelBuffer: pixels)
    let rep = NSBitmapImageRep(ciImage: image)
    guard let jpeg = rep.representation(using: .jpeg, properties: [.compressionFactor: 0.9]) else { exit(1) }
    do { try jpeg.write(to: URL(fileURLWithPath: out)) } catch { exit(1) }
    print("saved \(out) after \(seen) frames")
    exit(0)
  }
}

let session = AVCaptureSession()
session.sessionPreset = .high
guard let input = try? AVCaptureDeviceInput(device: device), session.canAddInput(input) else {
  FileHandle.standardError.write("cannot open \(device.localizedName)\n".data(using: .utf8)!)
  exit(1)
}
session.addInput(input)
let output = AVCaptureVideoDataOutput()
output.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA]
let grabber = Grabber(skip: skip, out: outPath)
output.setSampleBufferDelegate(grabber, queue: DispatchQueue(label: "camsnap"))
session.addOutput(output)
session.startRunning()
DispatchQueue.main.asyncAfter(deadline: .now() + 15) {
  FileHandle.standardError.write("timed out waiting for frames\n".data(using: .utf8)!)
  exit(1)
}
RunLoop.main.run()
