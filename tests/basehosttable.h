/*
 * Copyright (C) 2026 LogSquirl Contributors
 *
 * This file is part of logsquirl-custom-footer.
 *
 * logsquirl-custom-footer is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * logsquirl-custom-footer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with logsquirl-custom-footer.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "logsquirl_plugin_api.h"

#include <cstdlib>
#include <cstring>
#include <new>

namespace custom_footer {

/**
 * The base part of a host API table, as LogSquirl 26.10 passes it: in a
 * heap block of exactly LOGSQUIRL_HOST_API_BASE_SIZE bytes, so that any read
 * past the table is a read past the block, which AddressSanitizer reports.
 * The block is freed when the BaseHostTable goes, also when a test fails.
 */
class BaseHostTable {
public:
    /// Copies the base part of @p api.
    explicit BaseHostTable( const LogSquirlHostApi& api )
        : block_( static_cast<unsigned char*>( std::malloc( LOGSQUIRL_HOST_API_BASE_SIZE ) ) )
    {
        if ( !block_ ) {
            throw std::bad_alloc();
        }
        std::memcpy( block_, &api, LOGSQUIRL_HOST_API_BASE_SIZE );
    }

    ~BaseHostTable()
    {
        std::free( block_ );
    }

    BaseHostTable( const BaseHostTable& ) = delete;
    BaseHostTable& operator=( const BaseHostTable& ) = delete;

    /// The table, valid while this lives.
    const LogSquirlHostApi* api() const noexcept
    {
        return reinterpret_cast<const LogSquirlHostApi*>( block_ );
    }

private:
    unsigned char* const block_;
};

} // namespace custom_footer
