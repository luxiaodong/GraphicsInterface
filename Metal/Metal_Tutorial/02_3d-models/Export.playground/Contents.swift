import Cocoa
import PlaygroundSupport
import MetalKit

guard let device = MTLCreateSystemDefaultDevice() else {
    fatalError("GPU is not supported");
}

let frame = CGRect(x: 0, y: 0, width: 200, height: 200)
let view = MTKView(frame: frame, device: device);
view.clearColor = MTLClearColor(red: 0, green: 1, blue: 0, alpha: 1)

// 1
let allocator = MTKMeshBufferAllocator(device: device)
// 2
//let mdlMesh = MDLMesh(sphereWithExtent: [0.75, 0.75, 0.75],
//// 3
//    segments: [100, 100],
//    inwardNormals: false,
//    geometryType: .triangles,
//    allocator: allocator)

let mdlMesh = MDLMesh(coneWithExtent: [1,1,1],
                      segments: [10, 10],
                      inwardNormals: false,
                      cap: true,
                      geometryType: .triangles,
                      allocator: allocator)


// begin export code
// 1
let asset = MDLAsset()
asset.add(mdlMesh)
// 2
let fileType = "obj"
if MDLAsset.canExportFileExtension(fileType) {
    try? FileManager.default.createDirectory(at: playgroundSharedDataDirectory, withIntermediateDirectories: true, attributes: nil)
  let url = playgroundSharedDataDirectory.appendingPathComponent("primitive.obj")
  do {
    // 3
    try asset.export(to: url)
  } catch let error {
    // If you are getting this fatalError(), it may be because you
    // don't have a primitive.obj file in your Playground directory
    // `Documents\Shared Playground Data`
    fatalError("Error \(error.localizedDescription)")
  }
} else {
  fatalError("Can't export a .\(fileType) format")
}
// end export code





let mesh = try MTKMesh(mesh: mdlMesh, device: device)

let shader = """
#include <metal_stdlib>
using namespace metal;
struct VertexIn {
  float4 position [[ attribute(0) ]];
};
vertex float4 vertex_main(const VertexIn vertex_in [[ stage_in ]]) {
  return vertex_in.position;
}
fragment float4 fragment_main() {
  return float4(1, 0, 0, 1);
}
"""

let library = try device.makeLibrary(source: shader, options: nil)
let vertexFunction = library.makeFunction(name: "vertex_main")
let fragmentFunction = library.makeFunction(name: "fragment_main")

let descriptor = MTLRenderPipelineDescriptor()
descriptor.colorAttachments[0].pixelFormat = .bgra8Unorm
descriptor.vertexFunction = vertexFunction
descriptor.fragmentFunction = fragmentFunction
descriptor.vertexDescriptor =
MTKMetalVertexDescriptorFromModelIO(mesh.vertexDescriptor)

let pipelineState =
        try device.makeRenderPipelineState(descriptor: descriptor)

guard let commandQueue = device.makeCommandQueue() else {
  fatalError("Could not create a command queue")
}

// 1
guard let commandBuffer = commandQueue.makeCommandBuffer(),
// 2
let descriptor = view.currentRenderPassDescriptor,
// 3
let renderEncoder =
     commandBuffer.makeRenderCommandEncoder(descriptor: descriptor)
     else {  fatalError() }

renderEncoder.setRenderPipelineState(pipelineState)
renderEncoder.setVertexBuffer(mesh.vertexBuffers[0].buffer,
                             offset: 0, index: 0)

guard let submesh = mesh.submeshes.first else {
  fatalError()
}

renderEncoder.setTriangleFillMode(.lines)

renderEncoder.drawIndexedPrimitives(type: .triangle,
                          indexCount: submesh.indexCount,
                          indexType: submesh.indexType,
                          indexBuffer: submesh.indexBuffer.buffer,
                          indexBufferOffset: 0)

// 1
renderEncoder.endEncoding()
// 2
guard let drawable = view.currentDrawable else {
  fatalError()
}
// 3
commandBuffer.present(drawable)
commandBuffer.commit()

PlaygroundPage.current.liveView = view

1
var greeting = "Hello, playground"


