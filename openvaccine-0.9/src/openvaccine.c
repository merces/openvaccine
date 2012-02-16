/*
	OpenVaccine - utility to protect USB storage media against infections

	Copyright (C) 2011 - Fernando Mercês <fernando@mentebinaria.com.br>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <mntent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// macros
#define PROGRAM "OpenVaccine"
#define VERSION "0.9"
#define BANNER \
printf("\n%s %s\nby Fernando Mercês (fernando@mentebinaria.com.br)\n\n", PROGRAM, VERSION);

#define KILO 1024.0F
#define MEGA (KILO*KILO)
#define GIGA (MEGA*KILO)

typedef char CHAR;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef unsigned long QWORD;

// seta o alinhamento de estrututas para 1 byte (necessário para integridade dos structs)
#pragma pack(push, 1)

// FAT32 Boot Sector
typedef struct _FAT32_BOOTSECTOR
{
	BYTE	jmp[3];
	CHAR	OemName[8];
	WORD	BytesPerSector;
	BYTE	SectorsPerCluster;
	WORD	ReservedSectors;
	BYTE	NumberOfFATs;
	WORD	RootEntries;
	WORD	TotalSectors;
	BYTE	Media;
	WORD	SectorsPerFAT;
	WORD	SectorsPerTrack;
	WORD	HeadsPerCylinder;
	DWORD	HiddenSectors;
	DWORD	BigTotalSectors;
	DWORD	BigSectorsPerFAT;
	WORD	Flags;
	WORD	Version;
	DWORD	RootCluster;
	WORD	InfoSector;
	WORD	BootBackupStart;
	BYTE	Reserved[12];
	BYTE	DriveNumber;
	BYTE	Unused;
	BYTE	ExtBootSignature;
	DWORD	SerialNumber;
	CHAR	VolumeLabel[11];
	CHAR	FileSystem[8];
	BYTE	BootCode[422];
} FAT32_BOOTSECTOR;

// FAT-32 Data Directory
typedef struct _FAT_DATA_DIRECTORY
{
	BYTE Name[11];
	/*union
	{
		BYTE ReadOnly:1;
		BYTE Hidden:1;
		BYTE System:1;
		BYTE VolumeLabel:1;
		BYTE Subdirectory:1;
		BYTE Archive:1;
		BYTE Unused1:1;
		BYTE Unused2:1;
	} Attributes;*/
	BYTE Attributes;
	BYTE Reserved;
	BYTE TimeRes;
	WORD CreationTime;
	WORD CreationDate;
	WORD AccessTime;
	WORD EAIndex;
	WORD ModifiedTime;
	WORD ModifiedDate;
	WORD FirstCluster;
	DWORD FileSize;
} FAT_DATA_DIRECTORY;

#pragma pack(pop)

void usage(void)
{
	BANNER
	printf("Usage: openvaccine [partition]\nExample: openvaccine /dev/sdc1\n\n");
	exit(1);
}

/*
 retorna o ponto de montagem a partir do 'device' da partição
 agradecimentos a @computer4en6 e @andersonc0d3
*/
char *getmount(const char *partition)
{
	FILE *f = setmntent("/etc/mtab", "r");
	struct mntent *m;
	char *mntpnt = NULL;

	while ( (m = getmntent(f)) )
	{
		if (! strcmp(m->mnt_fsname, partition) )
		{
			mntpnt = m->mnt_dir;
			break;
		}
	}
	
	endmntent(f);
	return mntpnt;
}

void confirm(void)
{
	// função de confirmação (yes/no)
	char op;
	printf("Writing in low level has a risk.\nPlease, backup your data first. Continue (y/N)? ");
	scanf("%c", &op);
	printf("\n");
	if (tolower(op) != 'y')
		exit(0);
}

unsigned char lfn_checksum(const unsigned char *pFcbName)
{
	int i;
	unsigned char sum=0;

	for (i=11; i; i--)
		sum = ((sum & 1) << 7) + (sum >> 1) + *pFcbName++;

	return sum;
}

int main(int argc, char *argv[])
{
	FILE *fp, *mp;
	FAT32_BOOTSECTOR bs;
	FAT_DATA_DIRECTORY data;
	register int i;
	long long size;
	char *autorun_path;
	char *mountpoint;
	const char *file = "autorun.inf";
	const char s[] = "@MenteBinaria";

	// atributos 0x2 (oculto) e 0x40 (nao usado 1)
	const BYTE attr = 0x42;

	if (argv[1] == NULL || argc > 2)
		usage();
		
	if ( (mountpoint = getmount(argv[1])) == NULL)
	{
		fprintf(stderr, "partition %s not mounted\n", argv[1]);
		exit(1);
	}	

	// aloca memória para a variável que vai guardar o path do autorun.inf
	autorun_path = (char *) malloc(strlen(mountpoint) + strlen(file) + 1);
	sprintf(autorun_path, "%s/%s", mountpoint, file);	

	if ( (fp = fopen(argv[1], "r+b")) == NULL )
	{
		fprintf(stderr, "error opening partition %s\n", argv[1]);
		exit(1);
	}

	// lê o bootsector (primeiro setor da particao) para o struct bs
	if ( fread(&bs, sizeof(FAT32_BOOTSECTOR), 1, fp) != 1 )
	{
		fprintf(stderr, "error reading boot sector of partition %s\n", argv[1]);
		exit(1);
	}

	// testa se e uma particao FAT-32
	if (memcmp(bs.FileSystem, "FAT32", 5) )
	{
		fprintf(stderr, "partition %s is not FAT-32\n\n", argv[1]);
		exit(1);
	}

	// exibe informações sobre a partição
	BANNER
	printf("Partition %s mounted on %s\n + ", argv[1], mountpoint);

	for (i=0; isalnum(bs.FileSystem[i]); i++)
			printf("%c", bs.FileSystem[i]);

	printf(" (");
	for (i=0; isalnum(bs.OemName[i]); i++)
			printf("%c", bs.OemName[i]);

	size = bs.BigTotalSectors * bs.BytesPerSector;

	if (size < GIGA)
		printf(")\n + %0.0fM (%lld bytes)\n", size / MEGA, size);
	else
		printf(")\n + %0.0fG (%lld bytes)\n", size / GIGA, size);

	if (bs.NumberOfFATs > 1)
		printf(" + mirroring enabled\n");
		
	printf(" + %u sectors\n", bs.BigTotalSectors);
	printf(" + %hd bytes per sector\n", bs.BytesPerSector);
	printf(" + %dk cluster size\n", bs.SectorsPerCluster * bs.BytesPerSector / 1024);
	printf(" + serial is %u\n\n", bs.SerialNumber);
	
	// confirma antes de escrever na partição
	confirm();
	
	mp = fopen(autorun_path, "w");

	if (mp == NULL)
	{
		fprintf(stderr, "unable to write %s\n\n", autorun_path);
		exit(1);
	}

	fwrite(&s, strlen(s), 1, mp);
	free(autorun_path);
	fclose(mp);	

	/* o diretório de dados começa em "setores reservados + (setores por FAT * numero de FAT's)" mas
	 é preciso multiplicar por BytesPerSector para obter o setor e pular 32 bytes do cabeçalho do diretório */
	fseek(fp, 32 + (bs.ReservedSectors + (bs.BigSectorsPerFAT * bs.NumberOfFATs)) * bs.BytesPerSector, SEEK_SET);

	while (fread(&data, sizeof(FAT_DATA_DIRECTORY), 1, fp))
	{
		// lfn
		/*if ( data.Attributes == 0x0f &&
			data.TimeRes == lfn_checksum( (unsigned char *) "AUTORUN INF") )
		{
				BYTE z[sizeof(FAT_DATA_DIRECTORY)];
				
				memset(z, 0, sizeof(FAT_DATA_DIRECTORY));
				fseek(fp, - sizeof(FAT_DATA_DIRECTORY), SEEK_CUR);
				fwrite(&z, sizeof(FAT_DATA_DIRECTORY), 1, fp);
				times++;
				continue;
		}*/
	
		/* le cada entrada no diretorio de dados ate achar o autorun.inf
			recem-criado (TODO: checar CreatedTime) */

		if ( !strncasecmp( (char *)data.Name, "AUTORUN INF", sizeof(data.Name)) )
		//&& (data.FileSize == strlen(file)) )
		{
			/* posiciona no byte de atributos e grava attr */
			fseek(fp, - (sizeof(FAT_DATA_DIRECTORY) - sizeof(data.Name)), SEEK_CUR);
		
			if (fwrite(&attr, sizeof(BYTE), 1, fp))
			{
				unsigned long ofs, sector;

				ofs = ftell(fp) - sizeof(data.Name) - 1;
				sector = ofs / bs.BytesPerSector + 1;
				printf("%s created at sector %#lx, byte %#lx (offset %#lx).\n\n",
					file,
					sector,
					ofs - ((sector-1) * bs.BytesPerSector),
					ofs);
			}
			else
			{
				fprintf(stderr, "error setting attributes\n\n");
				exit(1);
			}
			break;
		}
	}

	fclose(fp);

	return 0;
}

