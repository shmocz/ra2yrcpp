// Based on this: https://github.com/hMihaiDavid/addscn
#include <cstring>

#include <memoryapi.h>

#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <windows.h>
#include <winnt.h>

static inline unsigned alignup(unsigned x, unsigned a) {
  return ((x + a - 1) / a) * a;
}

struct FileSize {
  unsigned int low;
  unsigned int high;
};

struct ImageHeaders {
  PIMAGE_DOS_HEADER dos;
  PIMAGE_NT_HEADERS nt;
  PIMAGE_FILE_HEADER file;
  PIMAGE_SECTION_HEADER first;
};

struct FileMapping {
  HANDLE hFileMapping{nullptr};
  void* pView{nullptr};
  std::unique_ptr<void, void (*)(void*)> hFile;

  explicit FileMapping(std::string path)
      : hFile(std::unique_ptr<void, void (*)(void*)>(
            CreateFile(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL),
            [](auto* d) { CloseHandle(d); })) {
    if (hFile.get() == INVALID_HANDLE_VALUE) {
      throw std::runtime_error("CreateFile() failed");
    }
  }

  ~FileMapping() {
    if (pView != nullptr && hFileMapping != nullptr) {
      unmap();
    }
  }

  FileSize size() {
    DWORD high = 0U;
    auto low = GetFileSize(hFile.get(), &high);
    return {low, high};
  }

  void map(DWORD flProtect, DWORD dwAccess, DWORD newSize = 0U) {
    if ((hFileMapping = CreateFileMapping(hFile.get(), NULL, flProtect, 0,
                                          newSize, NULL)) ==
        INVALID_HANDLE_VALUE) {
      hFileMapping = nullptr;
      throw std::runtime_error("failed to map file");
    }

    pView = MapViewOfFile(hFileMapping, dwAccess, 0, 0, 0);
  }

  ImageHeaders get_headers() {
    ImageHeaders I{(PIMAGE_DOS_HEADER)pView, NULL, NULL, NULL};
    I.nt =
        (PIMAGE_NT_HEADERS)(reinterpret_cast<char*>(pView) + I.dos->e_lfanew);
    I.file = &(I.nt->FileHeader);
    I.first = (PIMAGE_SECTION_HEADER)((reinterpret_cast<char*>(I.file) +
                                       sizeof(*I.file)) +
                                      I.nt->FileHeader.SizeOfOptionalHeader);
    return I;
  }

  bool unmap() {
    UnmapViewOfFile(pView);
    CloseHandle(hFileMapping);
    pView = nullptr;
    hFileMapping = nullptr;
    return true;
  }

  void append_section(std::string name, unsigned VirtualSize,
                      unsigned Characteristics) {
    auto H = get_headers();
    DWORD fileAlignment = H.nt->OptionalHeader.FileAlignment;
    DWORD imageBase = H.nt->OptionalHeader.ImageBase;
    auto sz = size();

    // remap
    unmap();
    map(PAGE_READWRITE, FILE_MAP_READ | FILE_MAP_WRITE,
        alignup(sz.low + VirtualSize, fileAlignment));

    // append section
    WORD numberOfSections = H.nt->FileHeader.NumberOfSections;
    DWORD sectionAlignment = H.nt->OptionalHeader.SectionAlignment;
    auto s_new = &H.first[numberOfSections];
    auto s_last = &H.first[numberOfSections - 1];

    memset(s_new, 0, sizeof(*s_new));
    memcpy(&s_new->Name, name.c_str(), std::min(name.size(), 8U));
    s_new->Misc.VirtualSize = VirtualSize;
    s_new->VirtualAddress = alignup(
        s_last->VirtualAddress + s_last->Misc.VirtualSize, sectionAlignment);
    s_new->SizeOfRawData = alignup(VirtualSize, fileAlignment);
    s_new->PointerToRawData = sz.low;
    s_new->Characteristics = Characteristics;

    H.nt->FileHeader.NumberOfSections = (numberOfSections + 1);
    H.nt->OptionalHeader.SizeOfImage = alignup(
        s_new->VirtualAddress + s_new->Misc.VirtualSize, sectionAlignment);

    memset((reinterpret_cast<char*>(pView) + s_new->PointerToRawData), 0,
           s_new->SizeOfRawData);

    std::cout << name.c_str() << ":0x" << std::hex << VirtualSize << ":0x"
              << std::hex << s_new->VirtualAddress + imageBase << ":0x"
              << std::hex << s_new->PointerToRawData << std::endl;
  }
};

void add_section(std::string path, std::string section_name,
                 unsigned int virtual_size, unsigned int characteristics) {
  FileMapping F(path);
  auto sz = F.size();
  if (sz.high != 0U) {
    throw std::runtime_error("large files not supported");
  }

  F.map(PAGE_READONLY, FILE_MAP_READ);

  auto H = F.get_headers();
#ifdef _WIN64
#define MACHINE IMAGE_FILE_MACHINE_AMD64
#else
#define MACHINE IMAGE_FILE_MACHINE_I386
#endif
  if (H.dos->e_magic != IMAGE_DOS_SIGNATURE ||
      H.nt->Signature != IMAGE_NT_SIGNATURE ||
      H.nt->FileHeader.Machine != MACHINE) {
    throw std::runtime_error("bad PE file");
  }

  F.append_section(section_name, virtual_size, characteristics);
}

int main(int argc, const char* argv[]) {
  if (argc < 5) {
    std::cerr
        << "usage: " << argv[0]
        << " <file> <section name> <VirtualSize> <Characteristics>" << std::endl
        << "Where VirtualSize is size of the section (hex or decimal), and "
           "Characteristics the COFF characteristics flag (hex number or "
           "decimal). Common flags: text: 0x60000020: "
           "data: 0xC0000040 rdata: 0x40000040"
        << std::endl;

    return EXIT_FAILURE;
  }

  try {
    add_section(argv[1], argv[2], std::stoul(argv[3], nullptr, 0),
                std::stoul(argv[4], nullptr, 0));
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
