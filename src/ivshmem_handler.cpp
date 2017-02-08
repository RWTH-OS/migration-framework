#include "ivshmem_handler.hpp"

#include <regex>
#include <iostream>

std::string add_ivshmem_dev(const std::string &xml, const std::string &id, const std::string &size, const std::string &path)
{
	/*
	std::string snippet_template = "\
<qemu:commandline>\n\
	<qemu:arg value='-chardev'/>\n\
	<qemu:arg value='socket,path=@path,id=@id'/>\n\
	<qemu:arg value='-device'/>\n\
	<qemu:arg value='ivshmem,chardev=@id,size=@size'/>\n\
</qemu:commandline>\n\
	";
	*/
	std::string snippet_template = "\
<shmem name='@id'>\n\
	<model type='ivshmem-plain'/>\n\
	<size unit='M'>@size</size>\n\
	<alias name='@id'/>\n\
</shmem>\n\
	";
	// TODO: Add unit option
	auto snippet = std::regex_replace(snippet_template, std::regex(R"((@id))"), id);
	snippet = std::regex_replace(snippet, std::regex(R"((@path))"), path);
	snippet = std::regex_replace(snippet, std::regex(R"((@size))"), size);
	std::cout << snippet << std::endl;

//	auto xml_ret = std::regex_replace(xml, std::regex(R"((</domain>))"), snippet + "</domain>");
	auto xml_ret = std::regex_replace(xml, std::regex(R"((</devices>))"), snippet + "</devices>");
	std::cout << xml_ret << std::endl;

	
	return xml_ret;
}

Ivshmem_handler::Ivshmem_handler()
{
	pre_migration();
}

Ivshmem_handler::~Ivshmem_handler() noexcept(false)
{
	try {
		post_migration();
	} catch (...) {
		if (!std::uncaught_exception())
			throw;
	}
}

void Ivshmem_handler::pre_migration()
{
	//TODO: pre-migration implementation
}

void Ivshmem_handler::post_migration()
{
	//TODO: post-migration implementation
}
