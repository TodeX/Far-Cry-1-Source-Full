/*=============================================================================
  VulkanRenderRE.cpp : implementation of the Rendering RenderElements pipeline.
  Copyright 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Jules

=============================================================================*/

#include "../RenderPCH.h"
#include "VulkanRenderer.h"
#include "I3dengine.h"

//=======================================================================

void CRE2DQuad::mfPrepare()
{
  gRenDev->EF_CheckOverflow(0, 0, this);

  gRenDev->m_RP.m_pRE = this;
  gRenDev->m_RP.m_RendNumIndices = 0;
  gRenDev->m_RP.m_FirstVertex = 0;
  gRenDev->m_RP.m_RendNumVerts = 4;

  int w=gRenDev->GetWidth();
  int h=gRenDev->GetHeight();

  m_arrVerts[0].xyz.x = (float)w;
  m_arrVerts[0].xyz.y = 0;
  m_arrVerts[0].xyz.z = 0;
  m_arrVerts[0].st[0] = 1;
  m_arrVerts[0].st[1] = 0;

  m_arrVerts[1].xyz.x = 0;
  m_arrVerts[1].xyz.y = 0;
  m_arrVerts[1].xyz.z = 0;
  m_arrVerts[1].st[0] = 0;
  m_arrVerts[1].st[1] = 0;

  m_arrVerts[2].xyz.x = (float)w;
  m_arrVerts[2].xyz.y = (float)h;
  m_arrVerts[2].xyz.z = 0;
  m_arrVerts[2].st[0] = 1;
  m_arrVerts[2].st[1] = 1;

  m_arrVerts[3].xyz.x = 0;
  m_arrVerts[3].xyz.y = (float)h;
  m_arrVerts[3].xyz.z = 0;
  m_arrVerts[3].st[0] = 0;
  m_arrVerts[3].st[1] = 1;
}

bool CRE2DQuad::mfDraw(SShader *ef, SShaderPass *sfm)
{
  // setup screen aligned quad...
  struct_VERTEX_FORMAT_P3F_TEX2F pScreenQuad[] =
  {
    Vec3(0, 0, 0), 0, 0,
    Vec3(0, 1, 0), 0, 1,
    Vec3(1, 0, 0), 1, 0,
    Vec3(1, 1, 0), 1, 1,
  };

  gRenDev->Set2DMode(true, 1, 1);
  // Create a temporary buffer and draw?
  // Using DrawTriStrip helper which creates temp buffer
  gRenDev->DrawTriStrip(&CVertexBuffer(pScreenQuad,VERTEX_FORMAT_P3F_TEX2F),4);
  gRenDev->Set2DMode(false, 1, 1);

  return true;
}

//=======================================================================

bool CREOcLeaf::mfPreDraw(SShaderPass *sl)
{
  CLeafBuffer *lb = m_pBuffer->GetVertexContainer();
  if (!lb->m_pVertexBuffer)
    return false;

  CVulkanRenderer *rd = gcpVulkan;

  // Bind Vertex Buffers
  // We assume standard streams for now.
  // Stream 0: Position/Normal/Color/TC0
  // Stream 1: Tangents
  // Stream 2: LM TC

  VkCommandBuffer cmd = rd->m_CommandBuffers[rd->m_CurrentFrame];

  // Stream 0
  int nOffs;
  VulkanBuffer* vb0 = (VulkanBuffer*)lb->m_pVertexBuffer->GetStream(VSF_GENERAL, &nOffs);
  if (vb0)
  {
      VkBuffer vertexBuffers[] = { vb0->buffer };
      VkDeviceSize offsets[] = { (VkDeviceSize)nOffs };
      vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
  }

  // Stream 1 (Tangents)
  if (rd->m_RP.m_FlagsModificators & RBMF_TANGENTSUSED)
  {
      VulkanBuffer* vb1 = (VulkanBuffer*)lb->m_pVertexBuffer->GetStream(VSF_TANGENTS, &nOffs);
      if (vb1)
      {
          VkBuffer vertexBuffers[] = { vb1->buffer };
          VkDeviceSize offsets[] = { (VkDeviceSize)nOffs };
          vkCmdBindVertexBuffers(cmd, 1, 1, vertexBuffers, offsets);
      }
  }

  // Stream 2 (Lightmap TC)
  if (rd->m_RP.m_FlagsModificators & RBMF_LMTCUSED)
  {
      if (rd->m_RP.m_pCurObject->m_pLMTCBufferO && rd->m_RP.m_pCurObject->m_pLMTCBufferO->m_pVertexBuffer)
      {
          VulkanBuffer* vb2 = (VulkanBuffer*)rd->m_RP.m_pCurObject->m_pLMTCBufferO->m_pVertexBuffer->GetStream(VSF_GENERAL, &nOffs);
          if (vb2)
          {
              VkBuffer vertexBuffers[] = { vb2->buffer };
              VkDeviceSize offsets[] = { (VkDeviceSize)nOffs };
              vkCmdBindVertexBuffers(cmd, 2, 1, vertexBuffers, offsets);
          }
      }
  }

  // Bind Index Buffer
  VulkanBuffer* ib = (VulkanBuffer*)m_pBuffer->m_Indices.m_VertBuf.m_pPtr;
  if (ib)
  {
      vkCmdBindIndexBuffer(cmd, ib->buffer, 0, VK_INDEX_TYPE_UINT16);
  }

  return true;
}

bool CREOcLeaf::mfDraw(SShader *ef, SShaderPass *sl)
{
  CLeafBuffer *lb = m_pBuffer;

  // For now we assume we always draw using indexed mesh if available
  gcpVulkan->EF_DrawIndexedMesh(lb->m_nPrimetiveType);

  return true;
}

void CREOcLeaf::mfEndFlush(void)
{
}
