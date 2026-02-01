#include "stdafx.h"

#include "utils.h"
#include "primitives.h"
#include "geometries.h"
#include "geoman.h"


void CGeomManager::InitGeoman()
{
	m_nGeomChunks = 0;
	m_nGeomsInLastChunk = 0;
}

void CGeomManager::ShutDownGeoman()
{
	int i, j;
	for(i=0; i<m_nGeomChunks; i++) {
		for(j=0; j<GEOM_CHUNK_SZ; j++)
			if (m_pGeoms[i][j].pGeom)
				delete m_pGeoms[i][j].pGeom;
		delete[] m_pGeoms[i];
	}
	if (m_nGeomChunks)
		delete[] m_pGeoms;
	m_nGeomChunks = 0;
	m_nGeomsInLastChunk = 0;
}


IGeometry *CGeomManager::CreateMesh(strided_pointer<const vectorf> pVertices, index_t *pIndices, const short *pIds, int nTris, int flags, bool bCopyTriangles,
																		bool bCopyVertices, float approx_tolerance, int nMinTrisPerNode, int nMaxTrisPerNode, float favorAABB)
{
	vectorr axes[3], center;
	primitive *pPrimitive;
	int i, itype = triangle::type;

	if (nTris <= 0)
		return 0;
	if (pIds)	{
		if (flags & mesh_uchar_ids) 
			for(i=1; i<nTris && ((char*)pIds)[i] == *(char*)pIds; i++);
		else
			for(i=1; i<nTris && pIds[i] == *pIds; i++);
	} else i = nTris;

	if (i == nTris) { // only try approximation if the mesh has one material
		ComputeMeshEigenBasis(pVertices, pIndices, nTris, axes, center);
		itype = ChoosePrimitiveForMesh(pVertices, pIndices, nTris, axes, center, flags, approx_tolerance, pPrimitive);
	}

	if (itype == triangle::type)
		return (new CTriMesh)->CreateTriMesh(pVertices, pIndices, pIds, nTris, flags, bCopyTriangles, bCopyVertices, nMinTrisPerNode, nMaxTrisPerNode, favorAABB);
	else
		return CreatePrimitive(itype, pPrimitive);
}


IGeometry *CGeomManager::CreatePrimitive(int type, const primitive *pPrimitive)
{
	switch (type) {
		case cylinder::type: return (new CCylinderGeom)->CreateCylinder((cylinder*)pPrimitive);
		case sphere::type: return (new CSphereGeom)->CreateSphere((sphere*)pPrimitive);
		case box::type: return (new CBoxGeom)->CreateBox((box*)pPrimitive);
		case heightfield::type: return (new CHeightfield)->CreateHeightfield((heightfield*)pPrimitive);
		case ray::type: return new CRayGeom((ray*)pPrimitive);
	}
	return 0;
}

void CGeomManager::DestroyGeometry(IGeometry *pGeom)
{
	pGeom->Release();
}


int CGeomManager::AddRefGeometry(phys_geometry *pPhysGeom)
{
	return ++pPhysGeom->nRefCount;
}

int CGeomManager::UnregisterGeometry(phys_geometry *pPhysGeom)
{
	if (--pPhysGeom->nRefCount != 0)
		return pPhysGeom->nRefCount;

	pPhysGeom->pGeom->Release();
	pPhysGeom->pGeom = 0;

	if (pPhysGeom - m_pGeoms[m_nGeomChunks-1] == m_nGeomsInLastChunk-1)
		m_nGeomsInLastChunk--;
	return 0;
}


phys_geometry *CGeomManager::GetFreeGeomSlot()
{
	int i, j;
	for(i=0; i<m_nGeomChunks-1; i++) {
		for(j=0; j<GEOM_CHUNK_SZ && m_pGeoms[i][j].pGeom; j++);
		if (j < GEOM_CHUNK_SZ) break;
	}
	if (i >= m_nGeomChunks-1)
		for(j=0; j<m_nGeomsInLastChunk && m_pGeoms[i][j].pGeom; j++);

	if (m_nGeomChunks == 0 || j == GEOM_CHUNK_SZ) {
		phys_geometry **t = m_pGeoms;
		m_pGeoms = new phys_geometry*[m_nGeomChunks+1];
		if (m_nGeomChunks) {
			memcpy(m_pGeoms, t, sizeof(phys_geometry*)*m_nGeomChunks);
			delete[] t;
		}
		i = m_nGeomChunks++; j = 0;
		memset(m_pGeoms[i] = new phys_geometry[GEOM_CHUNK_SZ], 0, sizeof(phys_geometry)*GEOM_CHUNK_SZ);
	}
	if (i == m_nGeomChunks-1 && j == m_nGeomsInLastChunk)
		m_nGeomsInLastChunk++;

	return m_pGeoms[i]+j;
}

phys_geometry *CGeomManager::RegisterGeometry(IGeometry *pGeom, int defSurfaceIdx)
{
	phys_geometry *pPhysGeom = GetFreeGeomSlot();
	pGeom->CalcPhysicalProperties(pPhysGeom);
	pPhysGeom->nRefCount = 1;
	pPhysGeom->surface_idx = defSurfaceIdx;
	return pPhysGeom;
}


void CGeomManager::SaveGeometry(CMemStream &stm, IGeometry *pGeom)
{
	stm.Write(pGeom->GetType());
	pGeom->Save(stm);
}

IGeometry *CGeomManager::LoadGeometry(CMemStream &stm)
{
	int itype; stm.Read(itype);
	IGeometry *pGeom;
	switch (itype) {
		case GEOM_TRIMESH: pGeom = new CTriMesh; break;
		case GEOM_CYLINDER: pGeom = new CCylinderGeom; break;
		case GEOM_SPHERE: pGeom = new CSphereGeom; break;
		case GEOM_BOX: pGeom = new CBoxGeom; break;
	}
	pGeom->Load(stm);
	return pGeom;
}

void CGeomManager::SavePhysGeometry(CMemStream &stm, phys_geometry *pPhysGeom)
{
	stm.Write(*pPhysGeom);
	SaveGeometry(stm, pPhysGeom->pGeom);
}

phys_geometry *CGeomManager::LoadPhysGeometry(CMemStream &stm)
{
	phys_geometry *pPhysGeom = GetFreeGeomSlot();
	stm.Read(*pPhysGeom);
	pPhysGeom->pGeom = LoadGeometry(stm);
	return pPhysGeom;
}
