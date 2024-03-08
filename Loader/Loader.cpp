// Loader.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "Utils.h"
#include "BootImage.h"
#include "ShareData.h"
#include "Cxxopts.hpp"

MapperContext g_MapperContext = { 0 };


 
int main(int argc,char** argv)
{
    cxxopts::Options options("Voyager-Next Loader", "Voyager-Next Loader");
    options.add_options()
        ("s,spoofer", "change smbios")
        ("d,seed", "seed", cxxopts::value<std::string>()->default_value("0"))
        ("p,syspath", "syspath", cxxopts::value<std::string>()->default_value(""))
        ;
    options.allow_unrecognised_options();
    auto result = options.parse(argc, argv);


#ifndef _DEBUG
    DeleteSelf();
#endif

    Call::CallFunction(KERNEL32, "LoadLibraryA", "shell32");
    Call::CallFunction(KERNEL32, "LoadLibraryA", "advapi32");

    {
	    if (!IsRunAsAdmin())
	    {
            TextPrint("Please run with admin! \n", ConsoleColor::Red);
            goto ENDP;
	    }
        BOOL secureBootState = 0;
        auto uefiBoot = UefiFirmware(secureBootState);
        if (!uefiBoot) {
            // 检查uefi
            TextPrint("Not UEFI system! \n", ConsoleColor::Red);
            goto ENDP;
        }
        else if (secureBootState) {
            // 检查 Secure boot
            TextPrint("Secure boot enabled! \n", ConsoleColor::Red);
            goto ENDP;
        }
    }



    {
        DisableKvas();
        EnableHyperV();
        DisableFastboot();
        // DeleteSvc("EventLog");
    }

    LockConsole(true);
    {
        // 获取当前运行目录
        wchar_t currentExe[MAX_PATH] = { 0 };
        GetModuleFileNameW(NULL, &currentExe[0], MAX_PATH);
        std::wstring currentPath(currentExe);
        auto pos = currentPath.find_last_of(L"\\");
        currentPath = currentPath.substr(0, pos);

        // 保存参数到Context
        g_MapperContext.test = RandomInt(1000,9999);
        auto param_spoofer = result["spoofer"].as<bool>();
        g_MapperContext.RunWithSpoofer = param_spoofer?1:0;
        auto param_seed = result["seed"].as<std::string>();
        g_MapperContext.RandomSeed = atoi(param_seed.c_str());
        TextPrint("magic is " + std::to_string(g_MapperContext.test) + "\n", Green);
        if (param_spoofer)
        {
            TextPrint("run with spoofer,seed is " + param_seed + "\n", Green);
        }
        memset(&g_MapperContext.postCallLoadSysPath[0], 0, 255);
        auto param_syspath = result["syspath"].as<std::string>();
        param_syspath = "C:\\Users\\c\\Desktop\\2\\R0Example.sys";
        if (param_syspath.length()>1 && param_syspath.length()<=250)
        {
            memcpy(&g_MapperContext.postCallLoadSysPath[0], param_syspath.c_str(), 255);
        }


        std::string bootfwPathA = GetBootfwPath();
        std::wstring bootfwPath = String2Wstring(bootfwPathA);
        TextPrint(L"Bootfw Path is " + bootfwPath + L"\n");

        // 保存文件路径和fbi
        wchar_t embb[] = L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
        auto bootmgfwPath = bootfwPath + embb;
        memcpy_s(g_MapperContext.bootmgfw.path, 255, embb, sizeof(embb));

        wchar_t emb[] = L"\\EFI\\Microsoft\\Boot";
        auto bootDirPath = bootfwPath + emb;
        memcpy_s(g_MapperContext.boot.path, 255, emb, sizeof(emb));

        wchar_t em[] = L"\\EFI\\Microsoft";
        auto bootParentPath = bootfwPath + em;
        memcpy_s(g_MapperContext.bootParent.path, 255, em, sizeof(em));
        
        auto bootmgfw = FFile(bootmgfwPath.c_str(), false);
        auto bootDir = FFile(bootDirPath.c_str(), true);
        auto bootParent = FFile(bootParentPath.c_str(), true);

        if (!bootmgfw.OpenFile() || !bootDir.OpenFile() || !bootParent.OpenFile())
        {
            TextPrint("Open boot file or dir failed! \n", ConsoleColor::Red);
            goto ENDP;
        }

        auto bootmgfwfbi = bootmgfw.GetFileBaseInfo();
        memcpy_s(&g_MapperContext.bootmgfw.fbi, sizeof(FILE_BASIC_INFORMATION), &bootmgfwfbi, sizeof(bootmgfwfbi));

        auto bootDirfbi = bootDir.GetFileBaseInfo();
        memcpy_s(&g_MapperContext.boot.fbi, sizeof(FILE_BASIC_INFORMATION), &bootDirfbi, sizeof(bootDirfbi));

        auto bootParentDirfbi = bootParent.GetFileBaseInfo();
        memcpy_s(&g_MapperContext.bootParent.fbi, sizeof(FILE_BASIC_INFORMATION), &bootParentDirfbi, sizeof(bootParentDirfbi));


        TextPrint("Save file information success. \n", Green);
        bootDir.CloseFile();
        bootParent.CloseFile();

    	// patch pe
        auto bootmgfw_buff = bootmgfw.ReadFile(20 * 1024 * 1024L); //20MB
        bootmgfw.CloseFile();
        PEFile bootmgfwPe(bootmgfw_buff.first);

        auto b = bootmgfwPe.GetSectionHeader(".fuck");
        if (b != NULL)
        {
	        // 已经patch
            TextPrint("Boot already patch! \n", Red);
            goto ENDP;
        }


        // 去除数字签名验证
        BootNoCertPatch(bootmgfw_buff.first, bootmgfw_buff.second);


        //bootkit pe 解析
        auto bootKitFilePath = currentPath + L"\\bootmgfw.efi";
        FFile bootKitFile(bootKitFilePath.c_str(), false);
        if (!bootKitFile.OpenFile())
        {
            TextPrint("Can not find bootkit file.", Red);
            goto ENDP;
        }
        auto bootKitBuffer = bootKitFile.ReadFile(0);
        PEFile bootkitPe(bootKitBuffer.first);


        // post call pe 写入context
        auto postCallPeFilePath = currentPath + L"\\PostCallDrv.sys";
        FFile postCallPeFile(postCallPeFilePath.c_str(), false);
        memset(&g_MapperContext.PostCallSysData[0], 0, sizeof(g_MapperContext.PostCallSysData));
        if (!postCallPeFile.OpenFile())
        {
            TextPrint("Can not find postcall file.", Red);
            goto ENDP;
        }
        auto postCallFileBuffer =  postCallPeFile.ReadFile();
        memcpy(&g_MapperContext.PostCallSysData[0], postCallFileBuffer.first, postCallFileBuffer.second);
        MemFree(postCallFileBuffer.first);
        postCallPeFile.CloseFile();

        //写入hyper-v patch
        auto hypervPatchFilePath = currentPath + L"\\HyperV-Intel.dll";
        FFile hypervPeFile(hypervPatchFilePath.c_str(), false);
        memset(&g_MapperContext.hvPayloadData[0], 0, sizeof(g_MapperContext.hvPayloadData));
        if (!hypervPeFile.OpenFile())
        {
            TextPrint("Can not find Hyper-V patch file.", Red);
            goto ENDP;
        }
        auto hypervPatchFileBuffer = hypervPeFile.ReadFile();
        memcpy(&g_MapperContext.hvPayloadData[0], hypervPatchFileBuffer.first, hypervPatchFileBuffer.second);
        MemFree(hypervPatchFileBuffer.first);
        hypervPeFile.CloseFile();



        // 备份bootmgfw.efi到bootmgfw.efi.bak
        FFile fwOrign(bootmgfwPath.c_str(), false);
        auto last = bootmgfwPath.find_last_of(L"\\");
        auto bakPath = bootmgfwPath.substr(0, last) + L"\\bootmgfw.bak.efi";

        FFile fwOrignBak(bakPath.c_str(), false);
        fwOrignBak.DeleteFileOrDir();
        if(!fwOrignBak.OpenFile(FALSE) || !fwOrign.OpenFile())
        {
	        TextPrint("Create bak file fail ! \n", ConsoleColor::Red);
            goto ENDP;
        }
        auto foBuffer = fwOrign.ReadFile();
        fwOrignBak.WriteFile(foBuffer.first, foBuffer.second);
        MemFree(foBuffer.first);
        fwOrignBak.CloseFile();
        fwOrign.CloseFile();
        wchar_t embbb[] = L"\\EFI\\Microsoft\\Boot\\bootmgfw.bak.efi";
        memset(&g_MapperContext.bootmgfwBackupPath[0], 0, sizeof(g_MapperContext.bootmgfwBackupPath));
        memcpy(&g_MapperContext.bootmgfwBackupPath[0], embbb,  sizeof(embbb));



        // 修改pe
        PatchBootPe(bootmgfw_buff, bootKitBuffer, &bootmgfwPe, &bootkitPe);
        TextPrint("Success patch!\n", Green);



        // 写入文件
        FFile wf2((LPCWSTR)bootmgfwPath.c_str(), false);
        wf2.OpenFile(true);
        wf2.WriteFile(bootmgfw_buff.first, bootmgfw_buff.second);
        wf2.CloseFile();
    }

    LockConsole(false);
    TextPrint("Success! \n", Green);
    TextPrint("Enter any key to reboot! \n", Green);
    system("pause");
    ForceReboot();
    
ENDP:
    LockConsole(false);
    system("pause");
    return 0;
}



