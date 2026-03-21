#pragma once
/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/* dbus-print-message.h  Utility function to print out a message
 *
 * Copyright (C) 2003 Philip Blundell <philb@gnu.org>
 * Copyright (C) 2003 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#ifndef DBUS_PRINT_MESSAGE_H
#define DBUS_PRINT_MESSAGE_H

#include <stdio.h>
#include <string.h>
#include <dbus/dbus.h>

void print_message(DBusMessage *message, dbus_bool_t literal);

void print_iter(DBusMessageIter *iter, dbus_bool_t literal, int depth = 0);

#endif /* DBUS_PRINT_MESSAGE_H */
