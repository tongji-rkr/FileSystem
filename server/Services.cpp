#include "Services.h"

using namespace std;

map<string, stringstream (Services::*)(stringstream &)> Services::command_service_map = {
    {"open", &Services::open_service},
    {"close", &Services::close_service},
    {"read", &Services::read_service},
    {"write", &Services::write_service},
    {"lseek", &Services::lseek_service},
    {"create", &Services::create_service},
    {"rm", &Services::rm_service},
    {"ls", &Services::ls_service},
    {"mkdir", &Services::mkdir_service},
    {"cd", &Services::cd_service},
    {"cat", &Services::cat_service},
    {"upload", &Services::upload_service},
    {"download", &Services::download_service}};
    

stringstream Services::open_service(stringstream &ss)
{
    string param1;
    string param2;
    stringstream send_str;
    ss >> param1 >> param2;
    if (param1 == "" || param2 == "")
    {
        send_str << "open [filename] [mode]\n";
        send_str << "invalid arguments" << endl;
        return send_str;
    }
    string fpath = param1;
    if (!Utility::is_number(param2))
    {
        send_str << "[mode] invalid" << endl;
        return send_str;
    }
    int mode = atoi(param2.c_str());

    // 调用
    FD fd = Kernel::Instance().Sys_Open(fpath, mode);
    // 打印结果
    send_str << "[ return ]:\n"
             << "fd=" << fd << endl;
    return send_str;
}

stringstream Services::close_service(stringstream &ss)
{
    string p1_fd;
    stringstream send_str;
    ss >> p1_fd;
    if (p1_fd == "")
    {
        send_str << "close [fd]\n";
        send_str << "invalid arguments" << endl;
        return send_str;
    }
    if (!Utility::is_number(p1_fd))
    {
        send_str << "[fd] should be a number" << endl;
        return send_str;
    }
    int fd = atoi(p1_fd.c_str());
    if (fd < 0)
    {
        send_str << "[fd] fd should be positive" << endl;
        return send_str;
    }
    // 调用 API
    int ret = Kernel::Instance().Sys_Close(fd);
    send_str << "[ return ]\n"
             << "ret=" << ret << endl;
    return send_str;
}

stringstream Services::read_service(stringstream &ss)
{
    string p1_fd;
    string p2_size;
    stringstream send_str;
    ss >> p1_fd >> p2_size;
    if (p1_fd == "" || p2_size == "")
    {
        send_str << "read [fd] [length]\n";
        send_str << "invalid arguments" << endl;

        return send_str;
    }
    if (!Utility::is_number(p1_fd))
    {
        send_str << "[fd] should be a number" << endl;

        return send_str;
    }
    if (!Utility::is_number(p2_size))
    {
        send_str << "[length] should be a number" << endl;

        return send_str;
    }
    int fd = atoi(p1_fd.c_str());
    if (fd < 0)
    {
        send_str << "[fd] should be positive" << endl;

        return send_str;
    }
    int size = atoi(p2_size.c_str());
    if (size <= 0 || size > 1024)
    {
        send_str << "[length] valid range:(0,1024]" << endl;

        return send_str;
    }
    // 调用 API
    char buf[1025];
    memset(buf, 0, sizeof(buf));
    int ret = Kernel::Instance().Sys_Read(fd, size, 1025, buf);
    // 结果返回
    send_str << "[ return ]:\n"
             << "ret=" << ret << endl
             << buf << endl;

    return send_str;
}

stringstream Services::write_service(stringstream &ss)
{
    string p1_fd = "";
    string p2_content = "";
    stringstream send_str;
    ss >> p1_fd >> p2_content;
    if (p1_fd == "")
    {
        send_str << "write [fd] [text]\n";
        send_str << "invalid arguments" << endl;
        return send_str;
    }
    if (!Utility::is_number(p1_fd))
    {
        send_str << "[fd] should be a number" << endl;
        return send_str;
    }
    int fd = atoi(p1_fd.c_str());
    if (fd < 0)
    {
        send_str << "[fd] should be positive" << endl;
        return send_str;
    }
    if (p2_content.length() > 1024)
    {
        send_str << "[text] text is too long" << endl;
        return send_str;
    }
    char buf[1025];
    memset(buf, 0, sizeof(buf));
    strcpy(buf, p2_content.c_str());
    int size = p2_content.length();
    // 调用 API
    int ret = Kernel::Instance().Sys_Write(fd, size, 1024, buf);
    // 打印结果
    send_str << "[ return ]\n"
             << "ret=" << ret << endl;
    return send_str;
}

stringstream Services::lseek_service(stringstream &ss)
{
    string fd, position, ptrname;
    stringstream send_str;
    ss >> fd >> position >> ptrname;
    if (fd == "" || position == "" || ptrname == "")
    {
        send_str << "lseek [fd] [position] [ptrname]";
        send_str << "invalid arguments" << endl;
        return send_str;
    }
    if (!Utility::is_number(fd))
    {
        send_str << "[fd] invalid" << endl;
        return send_str;
    }
    int fd_int = atoi(fd.c_str());
    if (!Utility::is_number(position))
    {
        send_str << "[position] invalid" << endl;
        return send_str;
    }
    int position_int = atoi(position.c_str());
    if (!Utility::is_number(ptrname))
    {
        send_str << "[ptrname] invalid" << endl;
        return send_str;
    }
    int ptrname_int = atoi(ptrname.c_str());
    User &u = Kernel::Instance().GetUser();
    u.u_error = NOERROR;
    u.u_ar0[0] = 0;
    u.u_ar0[1] = 0;
    u.u_arg[0] = fd_int;
    u.u_arg[1] = position_int;
    u.u_arg[2] = ptrname_int;
    FileManager &fimanag = Kernel::Instance().GetFileManager();
    fimanag.Seek();
    send_str << "[Results:]\n"
             << "u.u_ar0=" << u.u_ar0 << endl;
    return send_str;
}

stringstream Services::create_service(stringstream &ss)
{
    string filename;
    stringstream send_str;
    ss >> filename;
    if (filename == "")
    {
        send_str << "mkfile [filepath]";
        send_str << "invalid arguments" << endl;
        return send_str;
    }
    User &u = Kernel::Instance().GetUser();
    u.u_error = NOERROR;
    u.u_ar0[0] = 0;
    u.u_ar0[1] = 0;
    char filename_char[512];
    strcpy(filename_char, filename.c_str());
    u.u_dirp = filename_char;
    u.u_arg[1] = Inode::IRWXU;
    FileManager &fimanag = Kernel::Instance().GetFileManager();
    fimanag.Creat();
    send_str << "mkfile sucess" << endl;
    return send_str;
}

stringstream Services::cd_service(stringstream &ss)
{
    stringstream send_str;
    string param1;
    ss >> param1;
    if (param1 == "")
    {
        send_str << "cd [fpath]";
        send_str << "invalid arguments" << endl;
        return send_str;
    }
    // 调用
    User &u = Kernel::Instance().GetUser();
    u.u_error = NOERROR;
    char dirname[300] = {0};
    strcpy(dirname, param1.c_str());
    u.u_dirp = dirname;
    u.u_arg[0] = (unsigned long long)(dirname);
    FileManager &fimanag = Kernel::Instance().GetFileManager();
    fimanag.ChDir();
    // 打印结果
    send_str << "[result]:\n"
             << "now dir=" << dirname << endl;
    return send_str;
}

stringstream Services::rm_service(stringstream &ss)
{
    string filename;
    stringstream send_str;
    ss >> filename;
    if (filename == "")
    {
        send_str << "rm [filename]";
        send_str << "invalid arguments" << endl;
        return send_str;
    }
    User &u = Kernel::Instance().GetUser();
    u.u_error = NOERROR;
    u.u_ar0[0] = 0;
    u.u_ar0[1] = 0;
    char filename_char[512];
    strcpy(filename_char, filename.c_str());
    u.u_dirp = filename_char;
    FileManager &fimanag = Kernel::Instance().GetFileManager();
    fimanag.UnLink();
    send_str << "rm success" << endl;
    return send_str;
}

stringstream Services::ls_service(stringstream &ss)
{
    stringstream send_str;
    User &u = Kernel::Instance().GetUser();
    u.u_error = NOERROR;
    string cur_path = u.u_curdir;
    FD fd = Kernel::Instance().Sys_Open(cur_path, (File::FREAD));
    send_str << " cur_path:" << cur_path << endl;
    char buf[33] = {0};
    while (1)
    {
        if (Kernel::Instance().Sys_Read(fd, 32, 33, buf) == 0)
            break;
        else
        {
            // send_str << "cur_path:" << cur_path << endl << "buf:" << buf;
            DirectoryEntry *mm = (DirectoryEntry *)buf;
            if (mm->m_ino == 0)
                continue;
            send_str << mm->m_name << endl;
            
            memset(buf, 0, 32);
        }
    }
    Kernel::Instance().Sys_Close(fd);
    return send_str;
}

stringstream Services::mkdir_service(stringstream &ss)
{
    stringstream send_str;
    string path;
    ss >> path;
    if (path == "")
    {
        send_str << "mkdir [dirname]";
        send_str << "invalid arguments" << endl;
        return send_str;
    }
    send_str << "doing mkdir " << path << endl;
    int ret = Kernel::Instance().Sys_CreatDir(path);
    send_str << "mkdir success (ret=" << ret << ")" << endl;
    return send_str;
}

stringstream Services::cat_service(stringstream &ss)
{
    string p1_fpath;
    stringstream send_str;
    ss >> p1_fpath;
    if (p1_fpath == "")
    {
        send_str << "cat [filename]\n";
        send_str << "invalid arguments" << endl;
        return send_str;
    }
    string fpath = p1_fpath;
    // Open
    FD fd = Kernel::Instance().Sys_Open(fpath, 0x1);
    if (fd < 0)
    {
        send_str << "[cat] 打开文件出错." << endl;
        return send_str;
    }
    // Read
    char buf[257];
    while (true)
    {
        memset(buf, 0, sizeof(buf));
        int ret = Kernel::Instance().Sys_Read(fd, 256, 256, buf);
        if (ret <= 0)
        {
            break;
        }
        send_str << buf;
    }
    // Close
    Kernel::Instance().Sys_Close(fd);
    return send_str;
}

stringstream Services::upload_service(stringstream &ss)
{
    stringstream send_str;
    string p1_ofpath;
    string p2_ifpath;
    ss >> p1_ofpath >> p2_ifpath;
    if (p1_ofpath == "" || p2_ifpath == "")
    {
        send_str << "upload [localpath] [remotepath]\n";
        send_str << "invalid arguments" << endl;
        return send_str;
    }
    // 打开外部文件
    int ofd = open(p1_ofpath.c_str(), O_RDONLY); // 只读方式打开外部文件
    if (ofd < 0)
    {
        send_str << "[ERROR] failed to open file:" << p1_ofpath << endl;
        return send_str;
    }
    // 创建内部文件
    Kernel::Instance().Sys_Creat(p2_ifpath, 0x1 | 0x2);
    int ifd = Kernel::Instance().Sys_Open(p2_ifpath, 0x1 | 0x2);
    if (ifd < 0)
    {
        close(ofd);
        send_str << "[ERROR] failed to open file:" << p2_ifpath << endl;
        return send_str;
    }
    // 开始拷贝，一次 256 字节
    char buf[256];
    int all_read_num = 0;
    int all_write_num = 0;
    while (true)
    {
        memset(buf, 0, sizeof(buf));
        int read_num = read(ofd, buf, 256);
        if (read_num <= 0)
        {
            break;
        }
        all_read_num += read_num;
        int write_num =
            Kernel::Instance().Sys_Write(ifd, read_num, 256, buf);
        if (write_num <= 0)
        {
            send_str << "[ERROR] failed to write:" << p2_ifpath;
            break;
        }
        all_write_num += write_num;
    }
    send_str << "Bytes read:" << all_read_num
             << "Bytes written:" << all_write_num << endl;
    close(ofd);
    Kernel::Instance().Sys_Close(ifd);
    return send_str;
}

stringstream Services::download_service(stringstream &ss)
{
    string p1_ifpath;
    string p2_ofpath;
    stringstream send_str;
    ss >> p1_ifpath >> p2_ofpath;
    if (p1_ifpath == "" || p2_ofpath == "")
    {
        send_str << "download [remotepath] [localpath]\n";
        send_str << "invalid arguments" << endl;
        return send_str;
    }
    // 创建外部文件
    int ofd = open(p2_ofpath.c_str(), O_WRONLY | O_TRUNC | O_CREAT,777); // 截断写入方式打开外部文件
    if (ofd < 0)
    {
        send_str << "[ERROR] failed to create file:" << p2_ofpath << endl;
        return send_str;
    }
    // 打开内部文件
    int ifd = Kernel::Instance().Sys_Open(p1_ifpath, 0x1 | 0x2);
    if (ifd < 0)
    {
        close(ofd);
        send_str << "[ERROR] failed to open file:" << p1_ifpath << endl;
        return send_str;
    }
    // 开始拷贝，一次 256 字节
    char buf[256];
    int all_read_num = 0;
    int all_write_num = 0;
    while (true)
    {
        memset(buf, 0, sizeof(buf));
        int read_num =
            Kernel::Instance().Sys_Read(ifd, 256, 256, buf);
        if (read_num <= 0)
        {
            break;
        }
        all_read_num += read_num;
        int write_num = write(ofd, buf, read_num);
        if (write_num <= 0)
        {
            send_str << "[ERROR] failed to write:" << p1_ifpath;
            break;
        }
        all_write_num += write_num;
    }
    send_str << "Bytes read:" << all_read_num
             << "Bytes written:" << all_write_num << endl;
    close(ofd);
    Kernel::Instance().Sys_Close(ifd);
    return send_str;
}

Services &Services::Instance()
{
    static Services instance;
    return instance;
}

std::stringstream Services::process(const std::string &command, std::stringstream &ss,int& code)
{
    if (command_service_map.find(command) != command_service_map.end())
    {
        code=0;
        return (Instance().*(command_service_map[command]))(ss);
    }
    else
    {
        std::stringstream send_str;
        send_str << "[SERVICES] command "
                 << "\"" << command << "\""
                 << " not found" << std::endl;

        cout<<"[SERVICES] command "<< "\"" << command << "\""<<"not found"<<endl;
        code=-1;
        return send_str;
    }
}