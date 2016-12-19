/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef IVSHMEM_HANDLER_HPP
#define IVSHMEM_HANDLER_HPP

#include <string>

/**
 * \brief This function adds the qemu-command snippet to an xml string.
 */
std::string add_ivshmem_dev(const std::string &xml, const std::string &id, const std::string &size, const std::string &path);

/**
 * \brief This class handles the ivshmem device during migration.
 */
class Ivshmem_handler
{
	Ivshmem_handler();
	~Ivshmem_handler() noexcept(false);

	void pre_migration();
	void post_migration();
};

#endif
