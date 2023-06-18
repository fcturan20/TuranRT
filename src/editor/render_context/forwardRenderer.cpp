
// Color Attachment Info for Begin Render Pass
{
  colorAttachmentInfo.imageAccess = image_access_tgfx_SHADER_SAMPLEWRITE;
  colorAttachmentInfo.loadOp      = rasterpassLoad_tgfx_CLEAR;
  colorAttachmentInfo.storeOp     = rasterpassStore_tgfx_STORE;
  float cleardata[]               = {0.5, 0.5, 0.5, 1.0};
  memcpy(colorAttachmentInfo.clearValue.data, cleardata, sizeof(cleardata));

  depthAttachmentInfo.imageAccess    = image_access_tgfx_DEPTHREADWRITE_STENCILWRITE;
  depthAttachmentInfo.loadOp         = rasterpassLoad_tgfx_CLEAR;
  depthAttachmentInfo.loadStencilOp  = rasterpassLoad_tgfx_CLEAR;
  depthAttachmentInfo.storeOp        = rasterpassStore_tgfx_STORE;
  depthAttachmentInfo.storeStencilOp = rasterpassStore_tgfx_STORE;
  depthAttachmentInfo.texture        = ( struct tgfx_texture* )m_gpuCustomDepthRT->resource;
  *(( float* )depthAttachmentInfo.clearValue.data) = 1.0f;
}

// Create depth RT
{
  if (m_gpuCustomDepthRT) {
    rtRenderer::deallocateMemoryBlock(m_gpuCustomDepthRT);
  }

  tgfx_textureDescription textureDesc = {};
  textureDesc.channelType             = depthRTFormat;
  textureDesc.dataOrder               = textureOrder_tgfx_SWIZZLE;
  textureDesc.dimension               = texture_dimensions_tgfx_2D;
  textureDesc.resolution              = windowResolution;
  textureDesc.mipCount                = 1;
  textureDesc.permittedQueueCount     = swpchnDesc.permittedQueueCount;
  textureDesc.permittedQueues         = allQueues;
  textureDesc.usage = textureUsageMask_tgfx_RENDERATTACHMENT | textureUsageMask_tgfx_COPYFROM |
                      textureUsageMask_tgfx_COPYTO;

  m_gpuCustomDepthRT = allocateTexture(&textureDesc, getGpuMemRegion(rtMemoryRegion_LOCAL));
  depthAttachmentInfo.texture = ( struct tgfx_texture* )m_gpuCustomDepthRT->resource;
}