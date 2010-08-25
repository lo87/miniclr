////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft Corporation.  All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <tinyhal.h>
#include "FAT_FS.h"
#include "FAT_FS_Utility.h"
#include <TinyCLR_Interop.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FAT_LogicDisk member function
//
////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////
// Description:
//  read the data in FAT in cluster position
// 
// Input:
//   ClusIndex
//
// output:
//
// Remarks:
// 
// Returns:
UINT32 FAT_LogicDisk::ReadFAT( UINT32 clusIndex )
{
    if(clusIndex > m_totalClusterCount + 1)
        return CLUST_ERROR;
    
    UINT32 sectorsOffset = clusIndex / m_entriesPerSector;
    UINT32 dataOffset    = clusIndex % m_entriesPerSector;

    UINT32* fat = (UINT32*)SectorCache.GetSector( m_FATBaseSector[0] + sectorsOffset );

    if(!fat) return CLUST_ERROR;

    if(m_isFAT16)
    {
        UINT16* fat16 = (UINT16*)fat;

        return fat16[dataOffset];
    }
    else
    {
        return fat[dataOffset] & FAT32_MASK;
    }
}

/////////////////////////////////////////////////////////
// Description:
//  write the data in FAT in cluster position
// 
// Input:
//   ClusIndex
//
// output:
//
// Remarks:
// 
// Returns: old value
UINT32 FAT_LogicDisk::WriteFAT( UINT32 clusIndex, UINT32 value )
{
    if( clusIndex > m_totalClusterCount + 1)
        return CLUST_ERROR;

    UINT32  sectorsOffset = clusIndex / m_entriesPerSector;
    UINT32  dataOffset    = clusIndex % m_entriesPerSector;
    UINT32  oldValue;

    if(value == CLUST_EOF)
    {
        value = (m_isFAT16) ? CLUST_FAT16_EOFE : CLUST_FAT32_EOFE;
    }

    for(int i = 0; i < 2; i++)
    {
        UINT32* fat = (UINT32*)SectorCache.GetSector( m_FATBaseSector[i] + sectorsOffset, TRUE );
        if(!fat) return CLUST_ERROR;

        if(m_isFAT16)
        {
            UINT16* fat16 = (UINT16*)fat;

            oldValue = fat16[dataOffset];
            
            fat16[dataOffset] = value;
        }
        else
        {
            oldValue = fat[dataOffset] & FAT32_MASK;
            
            fat[dataOffset] = (value & FAT32_MASK) | (fat[dataOffset] & ~FAT32_MASK);
        }
    }

    // Update free count
    if(oldValue == CLUST_NONE)
    {
        m_freeCount--;
    }
    
    if(value == CLUST_NONE)
    {
        if(clusIndex < m_nextFree)
        {
            m_nextFree = clusIndex;
        }
        m_freeCount++;
    }

    if(!m_isFAT16)
    {
        FAT_FSINFO* fsInfo = (FAT_FSINFO*)SectorCache.GetSector( m_sectorFSInfo, TRUE );
        if(!fsInfo) return CLUST_ERROR;

        fsInfo->FSI_Free_Count = m_freeCount;
    }

    return oldValue;
}

/////////////////////////////////////////////////////////
// Description:
//      mount one formatted(FAT) disk 
// 
// Input:
//
// output:
//
// Remarks:
//      Parse DBR and set up disk
// 
// Returns:
BOOL FAT_LogicDisk::MountDisk()
{    
    BYTE dbrBuffer[DEFAULT_SECTOR_SIZE];

    FAT_DBR* dbr = (FAT_DBR*)&dbrBuffer[0];

    BOOL isFAT16;

    BOOL triedMBR = FALSE;

    while(TRUE)
    {
        if(!m_blockStorageDevice->Read( m_baseAddress, DEFAULT_SECTOR_SIZE, dbrBuffer ))
        {
            return FALSE;
        }

        if(dbr->IsValid( &isFAT16 ))
        {
            break;
        }
        else if(triedMBR)
        {
            return FALSE;
        }
        
        // Try MBR
        FAT_MBR* mbr = (FAT_MBR*)&dbrBuffer[0];

        if(!mbr->IsValid())
        {
            return FALSE;
        }

        UINT32 offset = mbr->Partitions[0].Get_RelativeSector() * DEFAULT_SECTOR_SIZE;
        UINT32 size = mbr->Partitions[0].Get_TotalSector() * DEFAULT_SECTOR_SIZE;

        if(offset + size > m_diskSize)
        {
            return FALSE;
        }

        m_baseAddress += offset;
        m_diskSize = size;

        triedMBR = TRUE;
    }

    InitMount( dbr, isFAT16 );

    
    UINT32 fatEntriesCount;

    if(isFAT16) 
    {
        fatEntriesCount = dbr->BPB_FATSz16 * m_bytesPerSector / sizeof(UINT16);
    }
    else
    {
        fatEntriesCount = dbr->DBRUnion.FAT32.BPB_FATSz32 * m_bytesPerSector / sizeof(UINT32);
    }

    //Sanity check for the sizes
    if((m_diskSize < m_totalSectorCount * m_bytesPerSector) || // Actual disk size has to be greater or equal to the size used by the FS
       (fatEntriesCount < (m_totalClusterCount + 1))) // FAT needs to fit all the clusters
    {
        return FALSE;
    }

    const BlockDeviceInfo* deviceInfo = m_blockStorageDevice->GetDeviceInfo();

    for(UINT32 i = 0; i < deviceInfo->NumRegions; i++)
    {
        //if block device block size is less than a FS sector, return error
        if(deviceInfo->Regions[i].BytesPerBlock < m_bytesPerSector)
        {
            return FALSE;
        }
    }
    
    return TRUE;
}

/////////////////////////////////////////////////////////
// Description:
//      Format one disk 
// 
// Input:
//      FormatType--now, reserved for FAT32
// output:
//
// Remarks:
// 
// Returns:
HRESULT FAT_LogicDisk::FormatHelper( UINT32 parameters )
{
    static const UINT32 DskTableFAT16[][2] =
    {
        {      8400,  0}, // <= 4.1 MB,     error
        {     32680,  2}, // <=  16 MB,  1k cluster
        {    262144,  4}, // <= 128 MB,  2k cluster
        {    524288,  8}, // <= 256 MB,  4k cluster
        {   1048576, 16}, // <= 512 MB,  8k cluster
        {   2097152, 32}, // <=   1 GB, 16k cluster
        {   4194304, 64}, // <=   2 GB, 32k cluster
        {0xFFFFFFFF,  0}, //  >   2 GB,     error
    };
    
    static const UINT32 DskTableFAT32[][2] = 
    {
        {     66600,  0}, // <= 32.5 MB,     error
        {    532480,  1}, // <=  260 MB, .5k cluster
        {  16777216,  8}, // <=    8 GB,  4k cluster
        {  33554432, 16}, // <=   16 GB,  8k cluster
        {  67108864, 32}, // <=   32 GB, 16k cluster
        {0xFFFFFFFF, 64}, //  >   32 GB, 32k cluster
    };

    //------//

    BYTE buffer[512];
    memset( buffer, 0, 512 );

    FAT_DBR* dbr = (FAT_DBR*)&(buffer[0]);

    //Jump instruction to boot code
    dbr->BS_JmpBoot[0] = 0xEB;
    dbr->BS_JmpBoot[1] = 0x3C;
    dbr->BS_JmpBoot[2] = 0x90;

    memcpy( dbr->BS_OEMName, "MSWIN4.1", 8 );
    
    dbr->Set_BPB_BytsPerSec( DEFAULT_SECTOR_SIZE );

    //This field is the new 32-bit total count of sectors on the volume
    UINT32 totSec = (UINT32)(m_diskSize / DEFAULT_SECTOR_SIZE);
    
    //Number of Sectors per allocation unit
    int i;
    UINT8 fat16SecPerClus = 0;
    UINT8 fat32SecPerClus = 0;

    // find the cluster size assuming FAT16
    for(i = 0; i < 8; i++)
    {
        if(totSec <= DskTableFAT16[i][0])
        {
            fat16SecPerClus = DskTableFAT16[i][1];
            break;
        }
    }

    // find the cluster size assuming FAT32
    for(i = 0; i < 6; i++)
    {
        if(totSec <= DskTableFAT32[i][0])
        {
            fat32SecPerClus = DskTableFAT32[i][1];
            break;
        }
    }

    if(fat16SecPerClus == 0 && fat32SecPerClus == 0)
    {
        return CLR_E_NOT_SUPPORTED;
    }

    BOOL useFAT16 = (fat16SecPerClus > 0 && fat16SecPerClus < 32);

    if(parameters & FORMAT_PARAMETER_FORCE_FAT16)
    {
        if(fat16SecPerClus == 0)
        {
            return CLR_E_NOT_SUPPORTED;
        }
        useFAT16 = TRUE;
    }
    else if(parameters & FORMAT_PARAMETER_FORCE_FAT32)
    {
        if(fat32SecPerClus == 0)
        {
            return CLR_E_NOT_SUPPORTED;
        }
        useFAT16 = FALSE;
    }

    dbr->BPB_SecPerClus = (useFAT16) ? fat16SecPerClus : fat32SecPerClus;

    if(totSec < 0x10000)
    {
        dbr->Set_BPB_TotSec16( totSec );
    }
    else
    {
        dbr->BPB_TotSec32 = totSec;
    }

    //Number of reserved sectors
    dbr->BPB_RsvdSecCnt = (useFAT16) ? 1 : 32; //FAT16 - 1 FAT32 -32
    
    //The count of FAT data structures on the volume 
    dbr->BPB_NumFATs = 2;

    //For FAT32 volumes, this field must be set to 0
    if(useFAT16)
    {
        dbr->Set_BPB_RootEntCnt( 512 ); //FAT16 - 512  FAT32 - 0
    }

    dbr->BPB_Media = 0xF8; //FlashDisk

    dbr->BPB_SecPerTrk = 63;
    dbr->BPB_NumHeads  = 32;
    dbr->BPB_HiddSec   = 0 ;

    UINT32 fatSz;
    UINT32 rootDirSectors;
    {
        rootDirSectors = (useFAT16) ? 32 : 0; // BPB_RootEntCnt * 32 + (BPB_BytsPerSec - 1)) / BPB_BytsPerSec
        UINT32 tmpVal1 = (UINT32)(totSec - (dbr->BPB_RsvdSecCnt + rootDirSectors));
        UINT32 tmpVal2 = (UINT32)((256 * dbr->BPB_SecPerClus) + 2); // (256 * BPB_SecPerClus) + BPB_NumFATs

        if(!useFAT16) tmpVal2 /= 2;

        fatSz = (tmpVal1 + (tmpVal2 - 1)) / tmpVal2;
    }

    if(useFAT16)
    {
        dbr->BPB_FATSz16 = (UINT16)fatSz;

        dbr->DBRUnion.FAT16.BS_DrvNum = 0x80;
        dbr->DBRUnion.FAT16.BS_BootSig = 0x29;

        FAT_Utility::GetCurrentFATTime( (UINT16*)&dbr->DBRUnion.FAT16.BS_VolID[2], (UINT16*)&dbr->DBRUnion.FAT16.BS_VolID[0], NULL );

        memcpy( dbr->DBRUnion.FAT16.BS_VolLab    , "MFDISK     ", 11 );
        memcpy( dbr->DBRUnion.FAT16.BS_FilSysType, "FAT16   "   ,  8 );
    }
    else
    {
        //----FAT32-------//
        dbr->DBRUnion.FAT32.BPB_FATSz32 = fatSz;

        dbr->DBRUnion.FAT32.BPB_ExtFlags = 0;
        dbr->DBRUnion.FAT32.BPB_FSVer = 0x0000;
        dbr->DBRUnion.FAT32.BPB_RootClus = 2;
        dbr->DBRUnion.FAT32.BPB_FSInfo = 1;
        dbr->DBRUnion.FAT32.BPB_BkBootSec = 6;


        dbr->DBRUnion.FAT32.BS_DrvNum = 0x80;
        dbr->DBRUnion.FAT32.BS_BootSig = 0x29;

        FAT_Utility::GetCurrentFATTime( (UINT16*)&dbr->DBRUnion.FAT32.BS_VolID[2], (UINT16*)&dbr->DBRUnion.FAT32.BS_VolID[0], NULL );

        memcpy( dbr->DBRUnion.FAT32.BS_VolLab    , "MFDISK     ", 11 );
        memcpy( dbr->DBRUnion.FAT32.BS_FilSysType, "FAT32   "   ,  8 );
    }

    dbr->EndingFlag = 0xAA55;

    InitMount( dbr, useFAT16 );

    //--//

    // Zero out all sectors first
    UINT32 sectorsNeeded = dbr->BPB_RsvdSecCnt + (2 * fatSz) + rootDirSectors; //BPB_RsvdSecCnt BPB_NumFATs * FATSz + RootDirSectors
    if(!useFAT16) sectorsNeeded += dbr->BPB_SecPerClus; // for the FAT32 root directory
    if(!m_blockStorageDevice->Memset( m_baseAddress, 0, sectorsNeeded * DEFAULT_SECTOR_SIZE ))
    {
        return CLR_E_FILE_IO;
    }
    
    // Write the first DBR
    if(!m_blockStorageDevice->Write( m_baseAddress, 512, buffer, TRUE ))
    {
        return CLR_E_FILE_IO;
    }

    // Write the second DBR and FSInfo for FAT32
    if(!useFAT16)
    {
        if(!m_blockStorageDevice->Write( m_baseAddress + 6 * DEFAULT_SECTOR_SIZE, 512, buffer, TRUE ))
        {
            return CLR_E_FILE_IO;
        }

        FAT_FSINFO* fsInfo = (FAT_FSINFO*)&(buffer[0]);
        
        fsInfo->Initialize( m_totalClusterCount - 1, 3 );

        if(!m_blockStorageDevice->Write( m_baseAddress + 1 * DEFAULT_SECTOR_SIZE, 512, buffer, TRUE ) ||
           !m_blockStorageDevice->Write( m_baseAddress + 7 * DEFAULT_SECTOR_SIZE, 512, buffer, TRUE ))
        {
            return CLR_E_FILE_IO;
        }
    }
    
    // Write the first entries of the FAT table
    memset( buffer, 0, 512 );

    if(useFAT16)
    {
        UINT16* fat16 = (UINT16*)&(buffer[0]);

        fat16[0] = 0xFFF8;
        fat16[1] = 0xFFFF;
    }
    else
    {
        UINT32* fat32 = (UINT32*)&(buffer[0]);

        fat32[0] = 0x0FFFFFF8;
        fat32[1] = 0x0FFFFFFF;
        fat32[2] = CLUST_FAT32_EOFE;
    }

    if(!m_blockStorageDevice->Write( m_baseAddress + m_FATBaseSector[0] * DEFAULT_SECTOR_SIZE, 512, buffer, TRUE ) ||
       !m_blockStorageDevice->Write( m_baseAddress + m_FATBaseSector[1] * DEFAULT_SECTOR_SIZE, 512, buffer, TRUE ))
    {
        return CLR_E_FILE_IO;
    }

    return S_OK;
}

/////////////////////////////////////////////////////////
// Description:
//     get the volLab of disk
// 
// Input:
//
// output:
//
// Remarks:
// 
// Returns:
HRESULT FAT_LogicDisk::GetDiskVolLab( LPSTR label )
{
    if(!label) return CLR_E_NULL_REFERENCE;

    FAT_EntryEnumerator entryEnum;
    FAT_FILE fileInfo;

    entryEnum.Initialize( this, m_rootSectorStart, 0 );

    while(fileInfo.Parse( this, &entryEnum ) == S_OK)
    {
        FAT_Directory* dirEntry = fileInfo.GetDirectoryEntry();

        if(dirEntry->DIR_Attr == ATTR_VOLUME_ID)
        {
            memcpy( label, dirEntry->DIR_Name, FAT_Directory::DIR_Name__size );
            label[FAT_Directory::DIR_Name__size] = '\0';

            return S_OK;
        }
    }

    label[0] = '\0';

    return S_OK;
}

/////////////////////////////////////////////////////////
// Description:
//     get the total size of disk
// 
// Input:
//
// output:
//
// Remarks:
// 
// Returns:
UINT64 FAT_LogicDisk::GetDiskTotalSize()
{
    return m_totalClusterCount * m_sectorsPerCluster * m_bytesPerSector;
}


/////////////////////////////////////////////////////////
// Description:
//     get the free size of disk
// 
// Input:
//
// output:
//
// Remarks:
// 
// Returns:
UINT64 FAT_LogicDisk::GetDiskFreeSize()
{
    return m_freeCount * m_sectorsPerCluster * m_bytesPerSector; 
}

/////////////////////////////////////////////////////////
// Description:
//     Converts Clusters to Sectors
// 
// Input:
//       Clusters
// output:
//      
// Remarks:
// 
// Returns:   
//       Sectors
UINT32 FAT_LogicDisk::ClusToSect( UINT32 clusIndex )
{
    if(clusIndex == CLUSTER_NOT_A_CLUSTER)
    {
        return m_rootSectorStart;
    }

    return (m_firstDataSector + (clusIndex - CLUSTER_START) * m_sectorsPerCluster);
}


/////////////////////////////////////////////////////////
// Description:
//     Converts Sectors to Clusters
// 
// Input:
//       Clusters
// output:
//      
// Remarks:
// 
// Returns:   
//       Sectors
UINT32 FAT_LogicDisk::SectToClus( UINT32 sectIndex )
{
    if(sectIndex < m_firstDataSector)
    {
        return CLUSTER_NOT_A_CLUSTER;
    }

    return (((sectIndex - m_firstDataSector) / m_sectorsPerCluster) + CLUSTER_START);
}

/////////////////////////////////////////////////////////
// Description:
//     in FAT, to find next free cluster
// 
// Input:
//       
// output:
//      
// Remarks:
// 
// Returns:   
//       next free cluster index
UINT32 FAT_LogicDisk::GetNextFreeClus( BOOL clear )
{
    UINT32 start      = m_nextFree;
    UINT32 maxCluster = m_totalClusterCount + 1;

    if(start > maxCluster)
    {
        start = CLUSTER_START;
    }

    UINT32 clusIndex = GetNextFreeClusHelper( start, maxCluster );

    // If we reach the end and still didn't find a free cluster, start from the beginning
    if(clusIndex == CLUST_ERROR)
    {
        clusIndex = GetNextFreeClusHelper( CLUSTER_START, start );
    }

    if(clusIndex != CLUST_ERROR)
    {
        m_nextFree = clusIndex + 1;

        if(!m_isFAT16)
        {
            FAT_FSINFO* fsInfo = (FAT_FSINFO*)SectorCache.GetSector( m_sectorFSInfo, TRUE );
            if(!fsInfo) return CLUST_ERROR;

            fsInfo->FSI_Nxt_free = m_nextFree;
        }

        if(clear) EraseCluster( clusIndex );
    }

    return clusIndex;
}

UINT32 FAT_LogicDisk::GetNextFreeClusHelper( UINT32 fromClus, UINT32 toClus )
{
    UINT32 sectorsOffset = fromClus / m_entriesPerSector;
    UINT32 dataOffset    = fromClus % m_entriesPerSector;

    UINT32 clusIndex = fromClus;
    
    UINT32* fat32;
    UINT16* fat16;

    while(TRUE)
    {
        fat32 = (UINT32*)SectorCache.GetSector( m_FATBaseSector[0] + sectorsOffset );
        if(!fat32) return CLUST_ERROR;

        fat16 = (UINT16*)fat32;

        while(dataOffset < m_entriesPerSector && clusIndex <= toClus)
        {
            if(( m_isFAT16 && fat16[dataOffset] == CLUST_NONE) ||
               (!m_isFAT16 && fat32[dataOffset] == CLUST_NONE))
            {
                return clusIndex;
            }

            dataOffset++;
            clusIndex ++;
        }

        dataOffset = 0;
        sectorsOffset++;
    }

    return CLUST_ERROR;
}

/////////////////////////////////////////////////////////
// Description:
//     in FAT, to find next cluster according to current cluster index
// 
// Input:
//   ClusIndex -- current cluster index
//   SectIndex -- current sector index
// output:
//   ClusIndex -- next cluster index(Maybe unchanged)
//   SectIndex -- next sector index
//      
// Remarks:
//        Flag--FAT_LogicDisk::GetNextSect__CREATE(This function is called by one Create function)
// Returns:   
//       
HRESULT FAT_LogicDisk::GetNextSect( UINT32 *clusIndex, UINT32 *sectIndex, UINT32 flag )
{
    UINT32 oldClusIndex = *clusIndex;
    UINT32 clus = *clusIndex;
    UINT32 sect = *sectIndex + 1;

    // For FAT16 Root directory
    if(oldClusIndex == CLUSTER_NOT_A_CLUSTER)
    {
        if(sect < m_firstDataSector)
        {
            *sectIndex = sect;
            return S_OK;
        }

        return CLR_E_FILE_IO;
    }
    
    if(SectToClus( sect ) == clus)
    {
        *sectIndex = sect;
    }
    else
    {
        clus = ReadFAT( clus );

        if(GetClusType( clus ) != CLUSTYPE_DATA)
        {
            if(flag & GetNextSect__CREATE)
            {
                clus = GetNextFreeClus( (flag & GetNextSect__CLEAR) ? TRUE : FALSE );
                
                if(clus == CLUST_ERROR)
                    return CLR_E_FILE_IO;

                WriteFAT( oldClusIndex, clus );
                WriteFAT( clus, CLUST_EOF );
            }
            else
            {
                return CLR_E_FILE_IO;
            }
        }

        *sectIndex = ClusToSect( clus );
        *clusIndex = clus;
    }

    
    return S_OK;
}

void FAT_LogicDisk::EraseCluster( UINT32 clusIndex )
{
    UINT32 sectIndex = ClusToSect( clusIndex );

    for(UINT32 i = 0; i < m_sectorsPerCluster; i++)
    {
        SectorCache.EraseSector( sectIndex++ );
    }
}

/////////////////////////////////////////////////////////////////////
// Description:
//     to find next free dir_entry location according to current sector index & offset
// 
// Input:
//   DataIndex -- current data offset in sector
//   SectIndex -- current sector index
//
// output:
//   DataIndex -- free dir-entry data offset in sector
//   SectIndex -- free sector index
//      
// Remarks:
//        Count--dir-entry count need to create one new dir-entry
// Returns:   
//       
HRESULT FAT_LogicDisk::GetNextFreeEntry( UINT32 *sectIndex, UINT32 *dataIndex, BYTE count )
{
    int num = 0;
    FAT_EntryEnumerator entryEnum;
    FAT_Directory* dirEntry;

    entryEnum.Initialize( this, *sectIndex, *dataIndex, TRUE );

    while((dirEntry = entryEnum.GetNext()) != NULL)
    {
        if(dirEntry->DIR_Name[0] == SLOT_DELETED || dirEntry->DIR_Name[0] == SLOT_EMPTY)
        {
            if(num == 0)
            {
                entryEnum.GetIndices( sectIndex, dataIndex );
            }

            num++;

            if(num >= count)
            {
                return S_OK;
            }
        }
        else
        {
            num = 0;
        }
    }


    return CLR_E_FILE_IO;
}

/////////////////////////////////////////////////////////////////////
// Description:
//     Parse cluster situation in FAT table
// 
// Input:
//
// output:
//      
// Remarks:
//       
// Returns:   
//       
UINT32 FAT_LogicDisk::GetClusType( UINT32 dwData )
{
    if(dwData == CLUST_ERROR)
        return CLUSTYPE_ERROR;

    if(m_isFAT16)
    {
        if(dwData >= CLUST_FAT16_EOFS)
            return CLUSTYPE_EOS;
        //EOS
        if(dwData == CLUST_FAT16_BAD)
            return CLUSTYPE_BAD;
    }
    else
    {
        if(dwData >= CLUST_FAT32_EOFS)
            return CLUSTYPE_EOS;
        //EOS
        if(dwData == CLUST_FAT32_BAD)
            return CLUSTYPE_BAD;
    }
    //BAD
    return CLUSTYPE_DATA; //Data 
}

/////////////////////////////////////////////////////////////////////
// Description:
//     in current directory to search one file entry use one name
// 
// Input:
//     ClusIndex-- current directory cluster index
//     FileName--
//
// output:
//      FileInfo -- parsed file info
// Remarks:
//       
// Returns:   
//       
BOOL FAT_LogicDisk::SearchCurDir( UINT32 clusIndex, LPCWSTR fileName, UINT32 fileNameLen, FAT_FILE* fileInfo )
{
    FAT_EntryEnumerator entryEnum;

    entryEnum.Initialize( this, ClusToSect( clusIndex ), 0 );

    while(fileInfo->Parse( this, &entryEnum ) == S_OK)
    {
        if(fileInfo->IsFileName( fileName, fileNameLen ) && fileInfo->GetDirectoryEntry()->DIR_Attr != ATTR_VOLUME_ID)
        {
            return TRUE;
        }
    }

    return FALSE;
}

/////////////////////////////////////////////////////////////////////
// Description:
//     search one file entry use whole path name
// 
// Input:
//     path-- whole path name
//
// output:
//      FileInfo -- parsed file info
// Remarks:
//       
// Returns:   
//      
FAT_Directory* FAT_LogicDisk::GetFile( LPCWSTR path, UINT32 pathLen, FAT_FILE* fileInfo, UINT32 flags )
{
    //root
    if(pathLen == 1 && path[0] == '\\')
    {
        return NULL;
    }
    
    LPCWSTR subPathPtr = NULL;
    UINT32 clusIndex = SectToClus( m_rootSectorStart );
    UINT32 fileLen;
    FAT_Directory* dirEntry = NULL;

    while(pathLen > 0)
    {
        // pass the initial "\"
        path++;
        pathLen--;

        // find the next "\"
        subPathPtr = FAT_Utility::FindChar( path, pathLen, '\\', &pathLen );

        fileLen = subPathPtr - path;

        // what's between the two is the file name of interest
        if(!SearchCurDir( clusIndex, path, fileLen, fileInfo ))
        {
            BYTE attributes;
            
            // we'll create the directory instead of failing if that was requested
            /**/ if((pathLen >  0 && (flags & GetFile__CREATE_PATH     )) || // if the flag is on, we'll create the path up to the final segment
                    (pathLen == 0 && (flags & GetFile__CREATE_DIRECTORY)))   // or if we're at the end, and was requested to create a directory
            {
                attributes = ATTR_DIRECTORY;
            }
            else if((pathLen == 0 && (flags & GetFile__CREATE_FILE     )))   // we're at the last segment, and was requested to create a file
            {
                attributes = ATTR_ARCHIVE;
            }
            else
            {
                return NULL;
            }

            if(fileInfo->Create( this, clusIndex, path, fileLen, attributes ) != S_OK)
            {
                return NULL;
            }
        }
        else if((pathLen == 0) && (flags & GetFile__FAIL_IF_EXISTS))
        {
            return NULL;
        }

        dirEntry = fileInfo->GetDirectoryEntry();
        if(!dirEntry) return NULL;

        // if we're not at the last segment, or we wants only directory 
        if((pathLen > 0) || (flags & GetFile__GET_DIRECTORY_ONLY))
        {
            //we need to make sure that it's a directory
            if((dirEntry->DIR_Attr & ATTR_DIRECTORY) != ATTR_DIRECTORY)
                return NULL;
        }

        // Update the clusIndex and path and continue
        clusIndex = dirEntry->GetFstClus();            
        path = subPathPtr; 
    }

    return dirEntry;
}

/////////////////////////////////////////////////////////////////////
// Description:
//     for initialize function, to get disk size(all BLOCKTYPE_FILESYSTEM blocks) 
// 
// Input:
//   
// output:
//   
// Remarks: now can support special path: Contiguous BLOCKTYPE_FILESYSTEM blocks arrange 
//              passing multi-regions and different region has different block size & sector size?
//       
// Returns:   
//      
BOOL FAT_LogicDisk::PopulateDiskSize()
{
    UINT32 j = 0;

    ByteAddress beginBlock;

    UINT32 regionIndex, rangeIndex;

    const BlockRegionInfo *pBlockRegionInfo;

    const BlockDeviceInfo *tmpBlockDeviceInfo = m_blockStorageDevice->GetDeviceInfo();

    
    //find sector address for first BLOCKTYPE_FILESYSTEM  block 
    if(m_blockStorageDevice->FindForBlockUsage(BlockUsage::FILESYSTEM, beginBlock, regionIndex, rangeIndex))
    {
        //in next regions to find the end of BLOCKTYPE_FILESYSTEM blocks
        pBlockRegionInfo = &tmpBlockDeviceInfo->Regions[regionIndex];

        m_baseAddress = pBlockRegionInfo->Start + (pBlockRegionInfo->BlockRanges[rangeIndex].StartBlock * pBlockRegionInfo->BytesPerBlock);

        //in begin region to find the end of BLOCKTYPE_FILESYSTEM blocks
        for(j = rangeIndex; j < pBlockRegionInfo->NumBlockRanges; j++)
        {
            const BlockRange* pRange = &pBlockRegionInfo->BlockRanges[j];

            if(!pRange->IsFileSystem())
            {
                m_sectorCount = (UINT32)(m_diskSize / tmpBlockDeviceInfo->BytesPerSector);
                return TRUE;
            }

            m_diskSize += pRange->GetBlockCount() * pBlockRegionInfo->BytesPerBlock;
        }

        m_sectorCount = (UINT32)(m_diskSize / tmpBlockDeviceInfo->BytesPerSector);
        
        return TRUE;
    }


    return FALSE;
}

FAT_LogicDisk* FAT_LogicDisk::Initialize( const VOLUME_ID * volume )
{
    FAT_LogicDisk* logicDisk = FAT_MemoryManager::AllocateLogicDisk( volume );

    if(!logicDisk)
    {
        return NULL;
    }

    //InitDisk return FALSE when there's no BLOCKTYPE_FILESYSTEM blocks
    if(!logicDisk->InitDisk( volume ) || !logicDisk->MountDisk())
    { 
        goto OnError;
    }

    if(!logicDisk->m_isFAT16)
    {
        FAT_FSINFO fsInfo;

        // Read FSINFO sector directly from device, as we might not be able to allocate memory yet if this is called during device boot up
        if(!logicDisk->m_blockStorageDevice->Read( logicDisk->m_baseAddress + logicDisk->m_sectorFSInfo * logicDisk->m_bytesPerSector, sizeof(FAT_FSINFO), (BYTE*)&fsInfo ) || 
            !fsInfo.IsValid())
        {
            goto OnError;
        }

        logicDisk->m_freeCount = fsInfo.FSI_Free_Count;
        logicDisk->m_nextFree  = fsInfo.FSI_Nxt_free;

        if(logicDisk->m_freeCount == 0xFFFFFFFF || logicDisk->m_freeCount > logicDisk-> m_totalClusterCount)
        {
            logicDisk->PopulateFreeCount();

            fsInfo.FSI_Free_Count = logicDisk->m_freeCount;
            fsInfo.FSI_Nxt_free   = logicDisk->m_nextFree;

            if(!logicDisk->m_blockStorageDevice->Write( logicDisk->m_baseAddress + logicDisk->m_sectorFSInfo * logicDisk->m_bytesPerSector, sizeof(FAT_FSINFO), (BYTE*)&fsInfo, TRUE ))
            {
                goto OnError;
            }
        }
    }
    else
    {
        logicDisk->PopulateFreeCount();
    }

    // Make sure we have a valid free count
    if(logicDisk->m_freeCount == 0xFFFFFFFF)
    {
        goto OnError;
    }

    logicDisk->SectorCache.Initialize( logicDisk->m_blockStorageDevice, logicDisk->m_bytesPerSector, logicDisk->m_baseAddress, logicDisk->m_sectorCount );

    return logicDisk;

OnError:
    FAT_MemoryManager::FreeLogicDisk( logicDisk );
    return NULL;        
}

void FAT_LogicDisk::PopulateFreeCount()
{
    BYTE buffer[512];

    m_freeCount = 0;
    m_nextFree  = 0;

    // try to read the first byte of the root directory sector
    if(!m_blockStorageDevice->Read( m_baseAddress + m_rootSectorStart * m_bytesPerSector, 1, buffer ))
    {
        m_freeCount = 0xFFFFFFFF;
        m_nextFree = 0;
        return;
    }

    // if the root directory is empty, we already know the free count
    if(buffer[0] == SLOT_EMPTY)
    {
        if(m_isFAT16)
        {
            m_freeCount = m_totalClusterCount;
            m_nextFree  = CLUSTER_START;
        }
        else
        {
            m_freeCount = m_totalClusterCount - 1;
            m_nextFree  = SectToClus( m_rootSectorStart ) + 1;
        }

        return;
    }

    // Brute force method: traverse through the entire FAT table
    
    UINT32 startAddress = m_baseAddress + m_FATBaseSector[0] * m_bytesPerSector;
    
    UINT32 c = 0;
    UINT32 lastIndex = m_totalClusterCount + 1;

    ::Watchdog_GetSetEnabled( FALSE, TRUE );

    while(TRUE)
    {
        if(!m_blockStorageDevice->Read( startAddress, 512, buffer ))
        {
            m_freeCount = 0xFFFFFFFF;
            m_nextFree = 0;

            ::Watchdog_GetSetEnabled( TRUE, TRUE );
            return;
        }

        startAddress += 512;

        if(m_isFAT16)
        {
            UINT16* fat = (UINT16*)&buffer[0];

            for(int i = 0; i < 512 / sizeof(UINT16); i++, c++)
            {
                if(fat[i] == CLUST_NONE)
                {
                    m_freeCount++;

                    if(m_nextFree == 0)
                    {
                        m_nextFree = c;
                    }
                }

                if(c == lastIndex)
                {
                    ::Watchdog_GetSetEnabled( TRUE, TRUE );
                    return;
                }
            }
        }
        else
        {
            UINT32* fat = (UINT32*)&buffer[0];

            for(int i = 0; i < 512 / sizeof(UINT32); i++, c++)
            {
                if(fat[i] == CLUST_NONE)
                {
                    m_freeCount++;
                    
                    if(m_nextFree == 0)
                    {
                        m_nextFree = c;
                    }
                }

                if(c == lastIndex)
                {
                    ::Watchdog_GetSetEnabled( TRUE, TRUE );
                    return;
                }
            }
        }
    }
}

// Clean up any allocated memory
BOOL FAT_LogicDisk::Uninitialize()
{
    UninitDisk();

    FAT_MemoryManager::FreeLogicDisk( this );

    return TRUE;
}

BOOL FAT_LogicDisk::IsLoadableMedia( BlockStorageDevice* driverInterface, UINT32* numVolumes )
{
    if(numVolumes == NULL)
        return FALSE;

    FAT_LogicDisk logicDisk;
    VOLUME_ID volume = { driverInterface, 0 };

    BOOL result = FALSE;

    //InitDisk return FALSE when there's no BLOCKTYPE_FILESYSTEM blocks
    if(logicDisk.InitDisk( &volume ) && logicDisk.MountDisk())
    {
        *numVolumes = 1;
        result = TRUE;
    }

    logicDisk.UninitDisk();
    
    return result;
}

HRESULT FAT_LogicDisk::Format( const VOLUME_ID *volume, UINT32 parameters )
{
    TINYCLR_HEADER();
    
    FAT_LogicDisk logicDisk;
    
    //InitDisk return FALSE when there's no BLOCKTYPE_FILESYSTEM blocks
    if(!logicDisk.InitDisk( volume ))
    {
        TINYCLR_SET_AND_LEAVE(CLR_E_FILE_IO);
    }

    TINYCLR_CHECK_HRESULT(logicDisk.FormatHelper( parameters ));


    TINYCLR_CLEANUP();

    logicDisk.UninitDisk(); // regardless of the result, need to uninitialize, or we leak memory

    TINYCLR_CLEANUP_END();
}

HRESULT FAT_LogicDisk::Open( LPCWSTR path, UINT32 *handle )
{
    TINYCLR_HEADER();
    
    UINT32 pathLen;

    FAT_Directory* dirEntry;
    FAT_FILE fileInfo;
    
    TINYCLR_CHECK_HRESULT(FAT_Utility::ValidatePathLength( path, &pathLen ));

    dirEntry = GetFile( path, pathLen, &fileInfo, GetFile__CREATE_FILE );

    if(!dirEntry)
    {
        TINYCLR_SET_AND_LEAVE(CLR_E_FILE_IO);
    }

    *handle = (UINT32)FAT_FileHandle::Open( this, &fileInfo, dirEntry->GetFstClus() );

    if(!(*handle))
    {
        TINYCLR_SET_AND_LEAVE(CLR_E_TOO_MANY_OPEN_HANDLES);
    }

    TINYCLR_NOCLEANUP();
}

HRESULT FAT_LogicDisk::FindOpen( LPCWSTR path, UINT32* findHandle )
{
    TINYCLR_HEADER();
    
    UINT32 pathLen;

    TINYCLR_CHECK_HRESULT(FAT_Utility::ValidatePathLength( path, &pathLen ));

    UINT32 clusIndex;

    //--//
    
    if(pathLen == 1 && path[0] == '\\')
    {
        clusIndex = CLUSTER_NOT_A_CLUSTER;
    }
    else
    {
        FAT_FILE fileInfo;
        FAT_Directory* dirEntry = GetFile( path, pathLen, &fileInfo, GetFile__GET_DIRECTORY_ONLY );
        
        if(!dirEntry)
        {
            TINYCLR_SET_AND_LEAVE(CLR_E_DIRECTORY_NOT_FOUND);
        }

        clusIndex = dirEntry->GetFstClus();
    }

    *findHandle = (UINT32)FAT_FINDFILES::FindOpen( this, clusIndex );
    
    if(!(*findHandle))
    {
        TINYCLR_SET_AND_LEAVE(CLR_E_TOO_MANY_OPEN_HANDLES);
    }

    TINYCLR_NOCLEANUP();
}

HRESULT FAT_LogicDisk::GetFileInfo( LPCWSTR path, FS_FILEINFO* fi, BOOL* found )
{
    TINYCLR_HEADER();
    
    UINT32 pathLen;

    TINYCLR_CHECK_HRESULT(FAT_Utility::ValidatePathLength( path, &pathLen ));

    // Special case for ROOT
    if(pathLen == 1 && path[0] == '\\')
    {
        fi->Attributes = ATTR_DIRECTORY;
        fi->CreationTime = 0;
        fi->LastAccessTime = 0;
        fi->LastWriteTime = 0;
        fi->Size = 0;
    }
    else
    {
        FAT_FILE fileInfo;
        FAT_Directory* dirEntry = GetFile( path, pathLen, &fileInfo );

        if(!dirEntry)
        {
            *found = FALSE;
            TINYCLR_SET_AND_LEAVE(S_OK);
        }

        fi->Attributes     = dirEntry->DIR_Attr;
        fi->Size           = dirEntry->DIR_FileSize;
        fi->CreationTime   = FAT_Utility::FATTimeToTicks( dirEntry->DIR_CrtDate   , dirEntry->DIR_CrtTime, dirEntry->DIR_CrtTimeTenth );
        fi->LastAccessTime = FAT_Utility::FATTimeToTicks( dirEntry->DIR_LstAccDate, 0                    , 0                          );
        fi->LastWriteTime  = FAT_Utility::FATTimeToTicks( dirEntry->DIR_WrtDate   , dirEntry->DIR_WrtTime, 0                          );
    }
    
    // Don't fill in the fileInfo->FileName, as it should be NULL (the caller already have that info .. it's the path parameter!)

    *found = TRUE;

    TINYCLR_NOCLEANUP();
}

HRESULT FAT_LogicDisk::GetAttributes( LPCWSTR path, UINT32* attributes )
{
    TINYCLR_HEADER();
    
    UINT32 pathLen;

    TINYCLR_CHECK_HRESULT(FAT_Utility::ValidatePathLength( path, &pathLen ));

    if(pathLen == 1 && path[0] == '\\')
    {
        *attributes = ATTR_DIRECTORY;
    }
    else
    {
        FAT_FILE fileInfo;
        FAT_Directory* dirEntry = GetFile( path, pathLen, &fileInfo );

        *attributes = (dirEntry) ? dirEntry->DIR_Attr : 0xFFFFFFFF;
    }

    TINYCLR_NOCLEANUP();
}

HRESULT FAT_LogicDisk::SetAttributes( LPCWSTR path, UINT32 attributes )
{
    TINYCLR_HEADER();
    
    UINT32 pathLen;
    FAT_FILE fileInfo;
    FAT_Directory* dirEntry;
    
    TINYCLR_CHECK_HRESULT(FAT_Utility::ValidatePathLength( path, &pathLen ));


    dirEntry = GetFile( path, pathLen, &fileInfo );

    if(!dirEntry)
    {
        TINYCLR_SET_AND_LEAVE(CLR_E_FILE_NOT_FOUND);
    }

    fileInfo.MarkDirectoryEntryForWrite();

    // We're allowing only setting of Readonly, hidden, system and archive attributes
    dirEntry->DIR_Attr &= ~ATTR_SET_MASK;
    dirEntry->DIR_Attr |= (BYTE)(attributes & ATTR_SET_MASK);
    
    TINYCLR_NOCLEANUP();
}

HRESULT FAT_LogicDisk::CreateDirectory( LPCWSTR path )
{
    TINYCLR_HEADER();
    
    UINT32 pathLen;    
    FAT_FILE fileInfo;

    TINYCLR_CHECK_HRESULT(FAT_Utility::ValidatePathLength( path, &pathLen ));

    if(pathLen >= FS_MAX_DIRECTORY_LENGTH)
    {
        TINYCLR_SET_AND_LEAVE(CLR_E_PATH_TOO_LONG);
    }

    // No-op for root
    if(pathLen == 1 && path[0] == '\\')
    {
        TINYCLR_SET_AND_LEAVE(S_OK);
    }

    if(!GetFile( path, pathLen, &fileInfo, GetFile__CREATE_PATH | GetFile__CREATE_DIRECTORY ))
    {
        TINYCLR_SET_AND_LEAVE(CLR_E_FILE_IO);
    }

    TINYCLR_NOCLEANUP();
}

HRESULT FAT_LogicDisk::Move( LPCWSTR oldPath, LPCWSTR newPath )
{
    TINYCLR_HEADER();
    
    UINT32 oldPathLen, newPathLen;    
    FAT_FILE srcFileInfo, destFileInfo;
    FAT_Directory* dirEntry;
    BYTE bytData[sizeof(FAT_Directory) - FAT_Directory::DIR_Name__size];

    TINYCLR_CHECK_HRESULT(FAT_Utility::ValidatePathLength( oldPath, &oldPathLen ));    
    TINYCLR_CHECK_HRESULT(FAT_Utility::ValidatePathLength( newPath, &newPathLen ));
    
    dirEntry = GetFile( oldPath, oldPathLen, &srcFileInfo );

    //check src path legal
    if(!dirEntry)
    {
        TINYCLR_SET_AND_LEAVE(CLR_E_FILE_NOT_FOUND);
    }

    // Store all the meta data, except the name
    memcpy( bytData, &dirEntry->DIR_Attr, sizeof(FAT_Directory) - FAT_Directory::DIR_Name__size );

    // Always create a file here, since we don't need it to allocate an extra cluster.
    if(!(dirEntry = GetFile( newPath, newPathLen, &destFileInfo, GetFile__CREATE_FILE | GetFile__FAIL_IF_EXISTS )))
    {
        TINYCLR_SET_AND_LEAVE(CLR_E_PATH_ALREADY_EXISTS);
    }

    // Copy the meta data from the temp storage
    destFileInfo.MarkDirectoryEntryForWrite();
    memcpy( (void*)(&dirEntry->DIR_Attr), bytData, sizeof(FAT_Directory) - FAT_Directory::DIR_Name__size );


    // Delete the old entry
    TINYCLR_CHECK_HRESULT(srcFileInfo.DeleteDirectoryEntry());

    TINYCLR_NOCLEANUP();
}

/////////////////////////////////////////////////////////////////////
// Description:
//     called by FAT_FS_Driver::delete 
// 
// Input:
//   
// output:
//   
// Remarks:
//       
// Returns:   
//      
HRESULT FAT_LogicDisk::Delete( LPCWSTR path )
{
    TINYCLR_HEADER();
    
    UINT32 pathLen;
    FAT_FILE fileInfo;
    UINT32 startClusIndex;
    FAT_Directory* dirEntry;

    TINYCLR_CHECK_HRESULT(FAT_Utility::ValidatePathLength( path, &pathLen ));

    if(pathLen == 1 && path[0] == '\\')
    {
        TINYCLR_SET_AND_LEAVE(CLR_E_FILE_IO);
    }

    //get file/path info to delete
    dirEntry = GetFile( path, pathLen, &fileInfo );
    
    if(!dirEntry)
    {
        TINYCLR_SET_AND_LEAVE(CLR_E_FILE_NOT_FOUND);
    }

    if(dirEntry->DIR_Attr & ATTR_READONLY)
    {
        TINYCLR_SET_AND_LEAVE(CLR_E_UNAUTHORIZED_ACCESS);
    }

    startClusIndex = dirEntry->GetFstClus();

    //is directory
    if((dirEntry->DIR_Attr & ATTR_DIRECTORY) == ATTR_DIRECTORY)
    {
        TINYCLR_CHECK_HRESULT(DeleteAll( startClusIndex ));
    }

    TINYCLR_CHECK_HRESULT(DeleteClusterChain( startClusIndex ));

    TINYCLR_CHECK_HRESULT(fileInfo.DeleteDirectoryEntry());

    TINYCLR_NOCLEANUP();
}

HRESULT FAT_LogicDisk::DeleteClusterChain( UINT32 clusIndex )
{
    UINT32 clusType;
    
    if(clusIndex != 0)
    {
        //clear cluster info in FAT table
        do
        {
            clusIndex = WriteFAT( clusIndex, CLUST_NONE );
            
            clusType = GetClusType( clusIndex );

        } while(clusType == CLUSTYPE_DATA);

        if(clusType == CLUSTYPE_ERROR || clusType == CLUSTYPE_BAD)
        {
            return CLR_E_FILE_IO;
        }
    }

    return S_OK;
}

HRESULT FAT_LogicDisk::DeleteAll( UINT32 clusIndex )
{
    TINYCLR_HEADER();
    
    FAT_FILE fileInfo;
    FAT_Directory* dirEntry;
    UINT32 startCluster;

    FAT_EntryEnumerator entryEnum;
    entryEnum.Initialize( this, ClusToSect( clusIndex ), 0 );
    
    //loop through all dirEntry
    while(fileInfo.Parse( this, &entryEnum ) == S_OK)
    {
        dirEntry = fileInfo.GetDirectoryEntry();
        if(!dirEntry) TINYCLR_SET_AND_LEAVE(CLR_E_FILE_IO);

        startCluster = dirEntry->GetFstClus();

        if((dirEntry->DIR_Attr & ATTR_DIRECTORY) == ATTR_DIRECTORY)
        {
            if(dirEntry->DIR_Name[0] == '.') continue;

            TINYCLR_CHECK_HRESULT(DeleteAll( startCluster ));
        }
        
        TINYCLR_CHECK_HRESULT(DeleteClusterChain( startCluster ));

        TINYCLR_CHECK_HRESULT(fileInfo.DeleteDirectoryEntry());
    }

    TINYCLR_NOCLEANUP();
}



//--//

BOOL FAT_LogicDisk::InitDisk( const VOLUME_ID* volume )
{
    memset(this, 0, sizeof(FAT_LogicDisk));

    if(!volume || !(volume->blockStorageDevice))
    {
        return FALSE;
    }
    
    m_volumeId           = volume->volumeId;
    m_blockStorageDevice = volume->blockStorageDevice;

    if(!PopulateDiskSize())
    {
        return FALSE;
    }

    return TRUE;
}

void FAT_LogicDisk::InitMount( FAT_DBR* dbr, BOOL isFAT16 )
{
    m_isFAT16           = isFAT16;
    m_bytesPerSector    = dbr->Get_BPB_BytsPerSec();
    m_sectorsPerCluster = dbr->BPB_SecPerClus;
    m_entriesPerSector  = m_bytesPerSector / ((isFAT16) ? 2 : 4);

    UINT32 rootDirSectors = ((dbr->Get_BPB_RootEntCnt() * 32) + (m_bytesPerSector - 1)) / m_bytesPerSector;
    UINT32 fatSz = (isFAT16) ? dbr->BPB_FATSz16 : dbr->DBRUnion.FAT32.BPB_FATSz32;
    UINT32 totSec = dbr->Get_BPB_TotSec16();
    if(totSec == 0) totSec = dbr->BPB_TotSec32;

    m_FATBaseSector[0]  = dbr->BPB_RsvdSecCnt;
    m_FATBaseSector[1]  = m_FATBaseSector[0] + fatSz;
    m_firstDataSector   = dbr->BPB_RsvdSecCnt + (fatSz * dbr->BPB_NumFATs) + rootDirSectors;

    m_totalClusterCount = (totSec - m_firstDataSector) / m_sectorsPerCluster;;
    m_totalSectorCount  = totSec;
    
    m_sectorFSInfo      = (isFAT16) ? 0 : dbr->DBRUnion.FAT32.BPB_FSInfo;
    m_rootSectorStart   = (isFAT16) ? m_firstDataSector - rootDirSectors : ClusToSect( dbr->DBRUnion.FAT32.BPB_RootClus );
}

void FAT_LogicDisk::UninitDisk()
{
    SectorCache.Uninitialize();
}

