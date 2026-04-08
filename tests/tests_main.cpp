/*
 * Copyright (C) 2026 LogSquirl Contributors
 *
 * This file is part of logsquirl-costume-footer.
 *
 * logsquirl-costume-footer is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * logsquirl-costume-footer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with logsquirl-costume-footer.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tests_main.cpp
 * @brief Catch2 + QApplication runner for the logsquirl-costume-footer test suite.
 */

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include <QApplication>

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );
    return Catch::Session().run( argc, argv );
}
