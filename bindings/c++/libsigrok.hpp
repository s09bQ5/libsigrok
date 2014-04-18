/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013-2014 Martin Ling <martin-sigrok@earth.li>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBSIGROK_HPP
#define LIBSIGROK_HPP

#include "libsigrok/libsigrok.h"
#include <glibmm-2.4/glibmm.h>

#include <stdexcept>
#include <memory>
#include <list>
#include <map>
#include <unordered_set>

namespace sigrok
{

using namespace std;

/* Forward declarations */
class SR_API Error;
class SR_API Context;
class SR_API Driver;
class SR_API Device;
class SR_API HardwareDevice;
class SR_API Channel;
class SR_API Session;
class SR_API ConfigKey;
class SR_API InputFormat;
class SR_API OutputFormat;
class SR_API LogLevel;
class SR_API ChannelGroup;
class SR_API ChannelType;
class SR_API Packet;
class SR_API PacketPayload;
class SR_API PacketType;
class SR_API Quantity;
class SR_API Unit;
class SR_API QuantityFlag;
class SR_API InputFileDevice;
class SR_API Output;
class SR_API DataType;

/** Exception thrown when an error code is returned by any libsigrok call. */
class SR_API Error: public exception
{
public:
	Error(int result);
	~Error() throw();
	const int result;
	const char *what() const throw();
};

/** Base template for most classes which wrap a struct type from libsigrok. */
template <class Parent, typename Struct> class SR_API StructureWrapper :
	public enable_shared_from_this<StructureWrapper<Parent, Struct> >
{
public:
	/** Parent object which owns this child object's underlying structure.

		This shared pointer will be null when this child is unused, but
		will be assigned to point to the parent before any shared pointer
		to this child is handed out to the user.

		When the reference count of this child falls to zero, this shared
		pointer to its parent is reset by a custom deleter on the child's
		shared pointer.

		This strategy ensures that the destructors for both the child and
		the parent are called at the correct time, i.e. only when all
		references to both the parent and all its children are gone. */
	shared_ptr<Parent> parent;
protected:
	Struct *structure;

	StructureWrapper<Parent, Struct>(Struct *structure) :
		structure(structure)
	{
	}
};

/** Context */
class SR_API Context : public enable_shared_from_this<Context>
{
public:
	/** Create new context */
	static shared_ptr<Context> create();
	/** libsigrok package version. */
	string get_package_version();
	/** libsigrok library version. */
	string get_lib_version();
	/** Available hardware drivers, indexed by name. */
	map<string, shared_ptr<Driver> > get_drivers();
	/** Available input formats, indexed by name. */
	map<string, shared_ptr<InputFormat> > get_input_formats();
	/** Available output formats, indexed by name. */
	map<string, shared_ptr<OutputFormat> > get_output_formats();
	/** Current log level. */
	const LogLevel *get_loglevel();
	/** Set the log level. */
	void set_loglevel(const LogLevel *level);
	/** Current log domain. */
	string get_logdomain();
	/** Set the log domain. */
	void set_logdomain(string value);
	/** Create a new session. */
	shared_ptr<Session> create_session();
protected:
	struct sr_context *structure;
	map<string, Driver *> drivers;
	map<string, InputFormat *> input_formats;
	map<string, OutputFormat *> output_formats;
	Session *session;
	Context();
	~Context();
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(Context *context) { delete context; }
	};
	friend class Deleter;
	friend class Session;
	friend class Driver;
};

/** Hardware driver */
class SR_API Driver : public StructureWrapper<Context, struct sr_dev_driver>
{
public:
	/** Name of this driver. */
	const string get_name();
	/** Long name for this driver. */
	const string get_longname();
	/** Scan for devices and return a list of devices found. */
	list<shared_ptr<HardwareDevice> > scan(
		map<const ConfigKey *, Glib::VariantBase> options);
protected:
	bool initialized;
	list<HardwareDevice *> devices;
	Driver(struct sr_dev_driver *structure);
	~Driver();
	friend class Context;
	friend class HardwareDevice;
	friend class ChannelGroup;
};

/** Generic device (may be real or from an input file) */
class SR_API Device : public StructureWrapper<Context, struct sr_dev_inst>
{
public:
	/** Vendor name for this device. */
	const string get_vendor();
	/** Model name for this device. */
	const string get_model();
	/** Version string for this device. */
	const string get_version();
	/** List of the channels available on this device. */
	list<shared_ptr<Channel> > get_channels();
	/** Open device. */
	void open();
	/** Close device. */
	void close();
protected:
	Device(struct sr_dev_inst *structure);
	~Device();
	list<Channel *> channels;
	friend class Session;
	friend class ChannelGroup;
	friend class Output;
};

/** Hardware device (connected via a driver) */
class SR_API HardwareDevice : public Device
{
public:
	/** Driver providing this device. */
	shared_ptr<Driver> get_driver();
	/** Channel groups available on this device, indexed by name. */
	map<string, shared_ptr<ChannelGroup> > get_channel_groups();
	/** Read device configuration for the given key. */
	Glib::VariantBase config_get(const ConfigKey *key);
	/** Set device configuration for the given key to a specified value. */
	void config_set(const ConfigKey *key, Glib::VariantBase value);
	/** Set device configuration for the given key, parsing a string input. */
	void config_set(const ConfigKey *key, string value);
	/** Enumerate available values for the given configuration key. */
	Glib::VariantBase config_list(const ConfigKey *key);
protected:
	HardwareDevice(Driver *driver, struct sr_dev_inst *structure);
	~HardwareDevice();
	Driver *driver;
	map<string, ChannelGroup *> channel_groups;
	friend class Driver;
	friend class ChannelGroup;
};

/** Channel */
class SR_API Channel : public StructureWrapper<Device, struct sr_channel>
{
public:
	/** Current name of this channel. */
	const string get_name();
	/** Type of this channel. */
	const ChannelType *get_type();
	/** Enabled status of this channel. */
	bool get_enabled();
	/** Set the enabled status of this channel. */
	void set_enabled(bool value);
protected:
	Channel(struct sr_channel *structure);
	~Channel();
	const ChannelType *type;
	friend class Device;
	friend class ChannelGroup;
};

/** Channel group */
class SR_API ChannelGroup :
	public StructureWrapper<HardwareDevice, struct sr_channel_group>
{
public:
	/** Name of this channel group. */
	const string get_name();
	/** List of the channels in this group. */
	list<shared_ptr<Channel> > get_channels();
	/** Read group configuration for the given key. */
	Glib::VariantBase config_get(const ConfigKey *key);
	/** Set group configuration for the given key to a specified value. */
	void config_set(const ConfigKey *key, Glib::VariantBase value);
	/** Set group configuration for the given key, parsing a string input. */
	void config_set(const ConfigKey *key, string value);
	/** Enumerate available values for the given configuration key. */
	Glib::VariantBase config_list(const ConfigKey *key);
protected:
	ChannelGroup(HardwareDevice *device, struct sr_channel_group *structure);
	~ChannelGroup();
	list<Channel *> channels;
	friend class HardwareDevice;
};

/** Type of datafeed callback */
typedef function<void(shared_ptr<Device>, shared_ptr<Packet>)> Callback;

/** Data required for C callback function to call a C++ callback */
class SR_PRIV CallbackData
{
public:
	void run(const struct sr_dev_inst *sdi,
		const struct sr_datafeed_packet *pkt);
protected:
	Callback callback;
	CallbackData(Session *session, Callback callback);
	Session *session;
	friend class Session;
};

/** Session */
class SR_API Session 
{
public:
	/** Add a device to this session. */
	void add_device(shared_ptr<Device> device);
	/** Add a datafeed callback to this session. */
	void add_callback(Callback callback);
	/** Start the session. */
	void start();
	/** Run the session event loop. */
	void run();
	/** Stop the session. */
	void stop();
protected:
	Session(shared_ptr<Context> context);
	~Session();
	struct sr_session *structure;
	const shared_ptr<Context> context;
	map<const struct sr_dev_inst *, shared_ptr<Device> > devices;
	list<CallbackData *> callbacks;
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(Session *session) { delete session; }
	};
	friend class Deleter;
	friend class Context;
	friend class CallbackData;
};

/** Datafeed packet */
class SR_API Packet
{
public:
	/** Payload of this packet. */
	PacketPayload *get_payload();
protected:
	Packet(const struct sr_datafeed_packet *structure);
	~Packet();
	const struct sr_datafeed_packet *structure;
	PacketPayload *payload;
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(Packet *packet) { delete packet; }
	};
	friend class Deleter;
	friend class Output;
	friend class CallbackData;
};

/** Abstract base class for datafeed packet payloads. */
class SR_API PacketPayload
{
protected:
	PacketPayload();
	virtual ~PacketPayload() = 0;
	virtual void *get_data() = 0;
	virtual size_t get_data_size() = 0;
	friend class Packet;
	friend class Output;
};

/** Logic data payload */
class SR_API Logic : public PacketPayload
{
protected:
	Logic(const struct sr_datafeed_logic *structure);
	~Logic();
	const struct sr_datafeed_logic *structure;
	vector<uint8_t> data;
	void *get_data();
	size_t get_data_size();
	friend class Packet;
};

/** Analog data payload */
class SR_API Analog : public PacketPayload
{
public:
	/** Number of samples in this packet. */
	unsigned int get_num_samples();
	/** Measured quantity of the samples in this packet. */
	const Quantity *get_mq();
	/** Unit of the samples in this packet. */
	const Unit *get_unit();
	/** Measurement flags associated with the samples in this packet. */
	unordered_set<const QuantityFlag *> get_mqflags();
protected:
	Analog(const struct sr_datafeed_analog *structure);
	~Analog();
	const struct sr_datafeed_analog *structure;
	void *get_data();
	size_t get_data_size();
	friend class Packet;
};

/** Input format */
class SR_API InputFormat :
	public StructureWrapper<Context, struct sr_input_format>
{
public:
	/** Name of this input format. */
	string get_name();
	/** Description of this input format. */
	string get_description();
	/** Check whether a given file matches this input format. */
	bool format_match(string filename);
	/** Open a file using this input format. */
	shared_ptr<InputFileDevice> open_file(string filename,
		map<string, string> options);
protected:
	InputFormat(struct sr_input_format *structure);
	~InputFormat();
	friend class Context;
	friend class InputFileDevice;
};

/** Virtual device associated with an input file */
class SR_API InputFileDevice : public Device
{
public:
	/** Load data from file. */
	void load();
protected:
	InputFileDevice(shared_ptr<InputFormat> format,
		struct sr_input *input, string filename);
	~InputFileDevice();
	struct sr_input *input;
	shared_ptr<InputFormat> format;
	string filename;
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(InputFileDevice *device) { delete device; }
	};
	friend class Deleter;
	friend class InputFormat;
};

/** Output format */
class SR_API OutputFormat :
	public StructureWrapper<Context, struct sr_output_format>
{
public:
	/** Name of this output format. */
	string get_name();
	/** Description of this output format. */
	string get_description();
	/** Create an output using this format. */
	shared_ptr<Output> create_output(shared_ptr<Device> device);
	/** Create an output using this format, passing an option string. */
	shared_ptr<Output> create_output(shared_ptr<Device> device, string option);
protected:
	OutputFormat(struct sr_output_format *structure);
	~OutputFormat();
	friend class Context;
	friend class Output;
};

/** Output instance (an output format applied to a device) */
class SR_API Output
{
public:
	/** Update output with data from the given packet. */
	string receive(shared_ptr<Packet> packet);
protected:
	Output(shared_ptr<OutputFormat> format, shared_ptr<Device> device);
	Output(shared_ptr<OutputFormat> format,
		shared_ptr<Device> device, string option);
	~Output();
	struct sr_output structure;
	const shared_ptr<OutputFormat> format;
	const shared_ptr<Device> device;
	const string option;
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(Output *output) { delete output; }
	};
	friend class Deleter;
	friend class OutputFormat;
};

/** Information about a configuration key */
class SR_API ConfigInfo
{
public:
	/* Configuration key to which this information applies. */
	const ConfigKey *get_key();
	/* Data type of the key. */
	const DataType *get_datatype();
	/* String identifier of the key. */
	string get_id();
	/* Name of the key. */
	string get_name();
	/* Description of the key. */
	string get_description();
protected:
	ConfigInfo(const struct sr_config_info *structure);
	~ConfigInfo();
	const struct sr_config_info *structure;
	friend class ConfigKey;
};

/** Base class for objects which wrap an enumeration value from libsigrok */
template <typename T> class SR_API EnumValue
{
public:
	/** The enum constant associated with this value. */
	T get_id() const { return id; }
	/** The name associated with this value. */
	string get_name() const { return name; }
protected:
	EnumValue(T id, const char name[]) : id(id), name(name) {}
	~EnumValue() {}
	const T id;
	const string name;
};

#include "enums.hpp"

}

#endif // LIBSIGROK_HPP
