#pragma once
#include "Pch.h"
typedef unsigned char	SMBIOS_TABLE_STRING;
typedef unsigned char	SMBIOS_TYPE;
typedef unsigned short	SMBIOS_HANDLE;


typedef struct {
	SMBIOS_TYPE    Type;
	UINT8          Length;
	SMBIOS_HANDLE  Handle;
} SMBIOS_STRUCTURE;


typedef struct {
	SMBIOS_STRUCTURE        Hdr;
	SMBIOS_TABLE_STRING     Manufacturer;
	SMBIOS_TABLE_STRING     ProductName;
	SMBIOS_TABLE_STRING     Version;
	SMBIOS_TABLE_STRING     SerialNumber;
	GUID                    Uuid;
	UINT8                   WakeUpType;
	SMBIOS_TABLE_STRING     SKUNumber;
	SMBIOS_TABLE_STRING     Family;
} SMBIOS_TABLE_TYPE1;