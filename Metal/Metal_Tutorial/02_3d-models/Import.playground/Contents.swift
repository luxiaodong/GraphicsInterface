import Cocoa
import PlaygroundSupport
import MetalKit

guard let device = MTLCreateSystemDefaultDevice() else {
    fatalError("GPU is not supported");
}

let frame = CGRect(x: 0, y: 0, width: 200, height: 200)
let view = MTKView(frame: frame, device: device);
view.clearColor = MTLClearColor(red: 1, green: 1, blue: 0, alpha: 1)

// 1
let allocator = MTKMeshBufferAllocator(device: device)
// 2
//let mdlMesh = MDLMesh(sphereWithExtent: [0.75, 0.75, 0.75],
//// 3
//    segments: [100, 100],
//    inwardNormals: false,
//    geometryType: .triangles,
//    allocator: allocator)

guard let assetURL = Bundle.main.url(forResource: "mushroom", withExtension: "obj") else {
  fatalError()
}

let vertexDescriptor = MTLVertexDescriptor()
vertexDescriptor.attributes[0].format = .float3
vertexDescriptor.attributes[0].offset = 0
vertexDescriptor.attributes[0].bufferIndex = 0

vertexDescriptor.layouts[0].stride = MemoryLayout<SIMD3<Float>>.stride
let meshDescriptor = MTKModelIOVertexDescriptorFromMetal(vertexDescriptor)
(meshDescriptor.attributes[0] as! MDLVertexAttribute).name = MDLVertexAttributePosition
let asset = MDLAsset(url: assetURL, vertexDescriptor: meshDescriptor, bufferAllocator: allocator)
let mdlMesh = asset.object(at: 0) as! MDLMesh
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

renderEncoder.setTriangleFillMode(.lines)

//guard let submesh = mesh.submeshes.first else {
//  fatalError()
//}
//
//renderEncoder.drawIndexedPrimitives(type: .triangle,
//                          indexCount: submesh.indexCount,
//                          indexType: submesh.indexType,
//                          indexBuffer: submesh.indexBuffer.buffer,
//                          indexBufferOffset: 0)

for submesh in mesh.submeshes {
    renderEncoder.drawIndexedPrimitives(
                            type: .triangle,
                            indexCount: submesh.indexCount,
                            indexType: submesh.indexType,
                            indexBuffer: submesh.indexBuffer.buffer,
                            indexBufferOffset: submesh.indexBuffer.offset)
}

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


