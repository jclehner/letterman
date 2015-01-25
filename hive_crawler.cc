#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <memory>
#include <vector>
#include "hive_crawler.h"
#include "exception.h"
#include "devtree.h"
#include "util.h"
using namespace std;

namespace letterman {
	namespace {
		struct ScopedMount
		{
			static ScopedMount create(const string& path)
			{
				ScopedMount ret;

				unique_ptr<char[]> tmpl(new char[32]);
				strcpy(tmpl.get(), "/tmp/lettermanXXXXXX");

				if (!mkdtemp(tmpl.get())) {
					throw ErrnoException("mkdtemp");
				}

				ret._target = tmpl.get();

#ifdef LETTERMAN_MACOSX
				struct NtfsMountOpts
				{
					const char* device;
					uint8_t majorVer;
					uint8_t minorVer;
				} __attribute__((packed));

				util::UniquePtrWithDeleter<NtfsMountOpts> opts(
						static_cast<NtfsMountOpts*>(valloc(sizeof(NtfsMountOpts))),
						[] (void* p) { free(p); });

				if (!opts) throw ErrnoException("valloc");

				*opts = {
					.device = path.c_str(),
					.majorVer = 0,
					.minorVer = 0 };

				if (mount("ntfs", ret._target.c_str(), 0, opts.get())) {
#else
				if (mount(path.c_str(), ret._target.c_str(), "ntfs", 0, NULL)) {
#endif
					throw ErrnoException("mount: " + path);
				}

				ret._path = path;

				return ret;
			}

			~ScopedMount()
			{
				if (!_target.empty()) {
#ifdef LETTERMAN_LINUX
					umount2(_target.c_str(), MNT_DETACH);
#else
					unmount(_target.c_str(), MNT_FORCE);
#endif
					rmdir(_target.c_str());
				}
			}

			const string& target() const 
			{ return _target; }

			private:
			string _path;
			string _target;
		};

		string findFirst(const string& path, string name, bool findDir = true)
		{
			DIR* dir = opendir(path.c_str());
			if (!dir) throw ErrnoException("opendir: " + path);

			util::capitalize(name);

			struct dirent d, *result;

			do
			{
				if (readdir_r(dir, &d, &result) != 0) {
					throw ErrnoException("readdir_r");
				}

				if (result) {
					string leaf(d.d_name);
					util::capitalize(leaf);

					if (leaf == name) {
						if (d.d_type == DT_UNKNOWN 
							|| d.d_type == (findDir ? DT_DIR : DT_REG)) {
							return path + "/" + d.d_name;
						}
					}
				}


			} while(result);

			throw UserFault(string("No such ") + (findDir ? "directory" : "file") + 
					" in " + path + ": " + name);
		}


	}

	set<WindowsInstall> getAllWindowsInstalls()
	{
		set<WindowsInstall> ret;
		Properties props = {{ DevTree::kPropIsNtfs, "1" }};

		for (auto& e : DevTree::getPartitions(props)) {
			try {
				WindowsInstall wi;

				wi.path = e.second[DevTree::kPropMountPoint];
				if (wi.path.empty()) {
					wi.isDevice = true;
					wi.path = e.second[DevTree::kPropDeviceMountable];
				} else {
					wi.isDevice = false;
				}

				// Ignore return value, just check!
				hiveFromSysDrive(wi.path);
				ret.insert(wi);
			} catch (const UserFault& e) {
				// ignore
			}
		}

		return ret;
	}

	string hiveFromSysDrive(const string& path)
	{
		struct stat st;

		if (stat(path.c_str(), &st) == -1) {
			if (errno != ENOENT) throw ErrnoException("stat");
			throw UserFault("Path does not exist: " + path);	
		}

		if (S_ISBLK(st.st_mode)) {
			if (geteuid() != 0) throw UserFault("Operation requires root");
			auto m(ScopedMount::create(path));
			return hiveFromSysDrive(m.target());
		} else if(!S_ISDIR(st.st_mode)) {
			throw UserFault("Not a device or directory: " + path);
		}

		return hiveFromSysRoot(findFirst(path, "Windows"));
	}

	inline string hiveFromSysRoot(const string& path)
	{
		return hiveFromSysDir(findFirst(path, "System32"));
	}

	inline string hiveFromSysDir(const string& path)
	{
		return findFirst(findFirst(path, "config"), "SYSTEM", false);
	}
}
