#include "User.h"
#include "Kernel.h"
#include <string.h>

void User::Setuid()	// 设置用户ID
{
	short uid = this->u_arg[0];
	
	if ( this->u_ruid == uid || this->SUser() )
	{
		this->u_uid = uid;
		this->u_ruid = uid;
	}
	else
	{
		this->u_error = EPERM;
	}
}

void User::Getuid() // 获取用户ID
{
	unsigned int uid;

	uid = (this->u_uid << 16);
	uid |= (this->u_ruid & 0xFF);
	this->u_ar0[User::EAX] = uid;
}

void User::Setgid() // 设置组ID
{
	short gid = this->u_arg[0];

	if ( this->u_rgid == gid || this->SUser() )
	{
		this->u_gid = gid;
		this->u_rgid = gid;
	}
	else
	{
		this->u_error = EPERM;
	}
}

void User::Getgid() // 获取组ID
{
	unsigned int gid;

	gid = (this->u_gid << 16);
	gid |= (this->u_rgid & 0xFF);
	this->u_ar0[User::EAX] = gid;
}

void User::Pwd() //	获取当前用户工作目录
{
	strcpy(this->u_curdir, this->u_dirp);
}

bool User::SUser() // 检查当前用户是否是超级用户
{
	if ( 0 == this->u_uid )
	{
		return true;
	}
	else
	{
		this->u_error = EPERM;
		return false;
	}
}
