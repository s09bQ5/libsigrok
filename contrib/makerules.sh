#!/bin/bash
##
## This file is part of the libsigrok project.
##
## Copyright (C) 2016 Daniel Glöckner <daniel-gl@gmx.net>
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
##

if [ $# -ne 2 ] ; then
	cat <<-EOF
	Usage: $0 <format> <input-file>

	Known formats are:
	   plugdev        udev rules assigning devices to group "plugdev"
	   uaccess        udev rules adding the tag "uaccess"
	   androidutils   xml file for sigrok-androidutils
	EOF
	exit 1
fi

case "$1" in
plugdev|uaccess)
	flavor=udev
	subflavor=$1
	;;

androidutils)
	flavor=$1
	;;
*)
	echo "Unknown output format \"$1\"" >&2
	exit 1
esac

udev_pre ()
{
	cat <<-EOF
	# This file has been automatically generated from $1

	ACTION!="add|change", GOTO="libsigrok_rules_end"
	SUBSYSTEM!="usb|usbmisc|usb_device", GOTO="libsigrok_rules_end"
	EOF
}

udev_post ()
{
	cat <<-EOF

	LABEL="libsigrok_rules_end"
	EOF
}

udev_comment ()
{
	echo "#$1"
}

udev_blank ()
{
	echo
}

udev_usbdev ()
{
	${subflavor}_usbdev "$@"
}

plugdev_usbdev ()
{
	echo "ATTRS{idVendor}==\"${1,,}\", ATTRS{idProduct}==\"${2,,}\", MODE=\"664\", GROUP=\"plugdev\""
}

uaccess_usbdev ()
{
	echo "ATTRS{idVendor}==\"${1,,}\", ATTRS{idProduct}==\"${2,,}\", TAG+=\"uaccess\""
}

androidutils_pre ()
{
	cat <<-EOF
	<?xml version="1.0" encoding="utf-8"?>
	<!-- This file has been automatically generated from $1 -->
	<resources>
	EOF
}

androidutils_post ()
{
	cat <<-EOF

	</resources>
	EOF
}

androidutils_comment ()
{
	echo "   <!-- $1 -->"
}

androidutils_blank ()
{
	echo
}

androidutils_usbdev ()
{
	echo "   <usb-device vendor-id=\"$((16#$1))\" product-id=\"$((16#$2))\" />"
}


${flavor}_pre "${2##*/}"

while read line ; do
	case "$line" in
	"##"*)
		;;
	"#"*)
		${flavor}_comment "${line#\#}"
		;;
	*:*)
		${flavor}_usbdev "${line%:*}" "${line#*:}"
		;;
	"")
		${flavor}_blank
		;;
	*)
		echo "Parse error" >&2
		exit 1
	esac
done < "$2"

${flavor}_post
