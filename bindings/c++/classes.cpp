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

#include "libsigrok.hpp"

namespace sigrok
{

/** Custom shared_ptr deleter for children owned by their parent object. */
template <class T> void reset_parent(T *child)
{
	child->parent.reset();
}

/** Helper function to translate C errors to C++ exceptions. */
static void check(int result)
{
	if (result != SR_OK)
		throw Error(result);
}

/** Helper function to obtain valid strings from possibly null input. */
static const char *valid_string(const char *input)
{
	if (input != NULL)
		return input;
	else
		return "";
}

Error::Error(int result) : result(result)
{
}

const char *Error::what() const throw()
{
	return sr_strerror(result);
}

Error::~Error() throw()
{
}

shared_ptr<Context> Context::create()
{
	return shared_ptr<Context>(new Context(), Context::Deleter());
}

Context::Context() :
	structure(structure),
	session(NULL)
{
	check(sr_init(&structure));
	struct sr_dev_driver **driver_list = sr_driver_list();
	if (driver_list)
		for (int i = 0; driver_list[i]; i++)
			drivers[driver_list[i]->name] =
				new Driver(driver_list[i]);
	struct sr_input_format **input_list = sr_input_list();
	if (input_list)
		for (int i = 0; input_list[i]; i++)
			input_formats[input_list[i]->id] =
				new InputFormat(input_list[i]);
	struct sr_output_format **output_list = sr_output_list();
	if (output_list)
		for (int i = 0; output_list[i]; i++)
			output_formats[output_list[i]->id] =
				new OutputFormat(output_list[i]);
}

string Context::get_package_version()
{
	return sr_package_version_string_get();
}

string Context::get_lib_version()
{
	return sr_lib_version_string_get();
}

map<string, shared_ptr<Driver>> Context::get_drivers()
{
	map<string, shared_ptr<Driver>> result;
	for (auto entry: drivers)
	{
		auto name = entry.first;
		auto driver = entry.second;
		driver->parent = static_pointer_cast<Context>(shared_from_this());
		result[name] = shared_ptr<Driver>(driver, reset_parent<Driver>);
	}
	return result;
}

map<string, shared_ptr<InputFormat>> Context::get_input_formats()
{
	map<string, shared_ptr<InputFormat>> result;
	for (auto entry: input_formats)
	{
		auto name = entry.first;
		auto input_format = entry.second;
		input_format->parent = static_pointer_cast<Context>(shared_from_this());
		result[name] = shared_ptr<InputFormat>(input_format,
			reset_parent<InputFormat>);
	}
	return result;
}

map<string, shared_ptr<OutputFormat>> Context::get_output_formats()
{
	map<string, shared_ptr<OutputFormat>> result;
	for (auto entry: output_formats)
	{
		auto name = entry.first;
		auto output_format = entry.second;
		output_format->parent = static_pointer_cast<Context>(shared_from_this());
		result[name] = shared_ptr<OutputFormat>(output_format,
			reset_parent<OutputFormat>);
	}
	return result;
}

Context::~Context()
{
	for (auto entry : drivers)
		delete entry.second;
	for (auto entry : input_formats)
		delete entry.second;
	for (auto entry : output_formats)
		delete entry.second;
	check(sr_exit(structure));
}

const LogLevel *Context::get_loglevel()
{
	return LogLevel::get(sr_log_loglevel_get());
}

void Context::set_loglevel(const LogLevel *level)
{
	check(sr_log_loglevel_set(level->get_id()));
}

string Context::get_logdomain()
{
	return valid_string(sr_log_logdomain_get());
}

void Context::set_logdomain(string value)
{
	check(sr_log_logdomain_set(value.c_str()));
}

shared_ptr<Session> Context::create_session()
{
	return shared_ptr<Session>(
		new Session(shared_from_this()), Session::Deleter());
}

Driver::Driver(struct sr_dev_driver *structure) :
	StructureWrapper<Context, struct sr_dev_driver>(structure),
	initialized(false)
{
}

Driver::~Driver()
{
	for (auto device : devices)
		delete device;
}

const string Driver::get_name()
{
	return valid_string(structure->name);
}

const string Driver::get_longname()
{
	return valid_string(structure->longname);
}

list<shared_ptr<HardwareDevice>> Driver::scan(
	map<const ConfigKey *, Glib::VariantBase> options)
{
	/* Initialise the driver if not yet done. */
	if (!initialized)
	{
		check(sr_driver_init(parent->structure, structure));
		initialized = true;
	}

	/* Clear all existing instances. */
	for (auto device : devices)
		delete device;
	devices.clear();

	/* Translate scan options to GSList of struct sr_config pointers. */
	GSList *option_list = NULL;
	for (auto entry : options)
	{
		auto key = entry.first;
		auto value = entry.second;
		auto config = g_new(struct sr_config, 1);
		config->key = key->get_id();
		config->data = value.gobj();
		option_list = g_slist_append(option_list, config);
	}

	/* Run scan. */
	GSList *device_list = sr_driver_scan(structure, option_list);

	/* Free option list. */
	g_slist_free_full(option_list, g_free);

	/* Create device objects. */
	for (GSList *device = device_list; device; device = device->next)
	{
		auto sdi = (struct sr_dev_inst *) device->data;
		devices.push_back(new HardwareDevice(this, sdi));
	}

	/* Free GSList returned from scan. */
	g_slist_free(device_list);

	/* Create list of shared pointers to device instances for return. */
	list<shared_ptr<HardwareDevice>> result;
	for (auto device : devices)
	{
		device->parent = parent->shared_from_this();
		result.push_back(shared_ptr<HardwareDevice>(device,
			reset_parent<HardwareDevice>));
	}
	return result;
}

Device::Device(struct sr_dev_inst *structure) :
	StructureWrapper<Context, struct sr_dev_inst>(structure)
{
	for (GSList *entry = structure->channels; entry; entry = entry->next)
	{
		auto channel = (struct sr_channel *) entry->data;
		channels.push_back(new Channel(channel));
	}
}

Device::~Device()
{
	for (auto channel : channels)
		delete channel;
}

const string Device::get_vendor()
{
	return valid_string(structure->vendor);
}

const string Device::get_model()
{
	return valid_string(structure->model);
}

const string Device::get_version()
{
	return valid_string(structure->version);
}

list<shared_ptr<Channel>> Device::get_channels()
{
	list<shared_ptr<Channel>> result;
	for (auto channel : channels)
	{
		channel->parent = static_pointer_cast<Device>(shared_from_this());
		result.push_back(shared_ptr<Channel>(channel, reset_parent<Channel>));
	}
	return result;
}

void Device::open()
{
	check(sr_dev_open(structure));
}

void Device::close()
{
	check(sr_dev_close(structure));
}

HardwareDevice::HardwareDevice(Driver *driver, struct sr_dev_inst *structure) :
	Device(structure),
	driver(driver)
{
	for (GSList *entry = structure->channel_groups; entry; entry = entry->next)
	{
		auto group = (struct sr_channel_group *) entry->data;
		channel_groups[group->name] = new ChannelGroup(this, group);
	}
}

HardwareDevice::~HardwareDevice()
{
	for (auto entry : channel_groups)
		delete entry.second;
}

shared_ptr<Driver> HardwareDevice::get_driver()
{
	return static_pointer_cast<Driver>(driver->shared_from_this());
}

map<string, shared_ptr<ChannelGroup>>
HardwareDevice::get_channel_groups()
{
	map<string, shared_ptr<ChannelGroup>> result;
	for (auto entry: channel_groups)
	{
		auto name = entry.first;
		auto channel_group = entry.second;
		channel_group->parent =
			static_pointer_cast<HardwareDevice>(shared_from_this());
		result[name] = shared_ptr<ChannelGroup>(channel_group,
			reset_parent<ChannelGroup>);
	}
	return result;
}

Glib::VariantBase HardwareDevice::config_get(const ConfigKey *key)
{
	GVariant *data;
	check(sr_config_get(driver->structure, structure, NULL, key->get_id(), &data));
	return Glib::VariantBase(data);
}

void HardwareDevice::config_set(const ConfigKey *key, Glib::VariantBase value)
{
	check(sr_config_set(structure, NULL, key->get_id(), value.gobj()));
}

void HardwareDevice::config_set(const ConfigKey *key, string value)
{
	auto variant = key->parse_string(value).gobj();
	check(sr_config_set(structure, NULL, key->get_id(), variant));
}

Glib::VariantBase HardwareDevice::config_list(const ConfigKey *key)
{
	GVariant *data;
	check(sr_config_list(driver->structure, structure, NULL, key->get_id(), &data));
	return Glib::VariantBase(data);
}

Channel::Channel(struct sr_channel *structure) :
	StructureWrapper<Device, struct sr_channel>(structure)
{
}

Channel::~Channel()
{
}

const string Channel::get_name()
{
	return valid_string(structure->name);
}

const ChannelType *Channel::get_type()
{
	return ChannelType::get(structure->type);
}

bool Channel::get_enabled()
{
	return structure->enabled;
}

void Channel::set_enabled(bool value)
{
	structure->enabled = value;
}

ChannelGroup::ChannelGroup(HardwareDevice *device,
		struct sr_channel_group *structure) :
	StructureWrapper<HardwareDevice, struct sr_channel_group>(structure)
{
	for (GSList *entry = structure->channels; entry; entry = entry->next)
	{
		auto channel = (struct sr_channel *) entry->data;
		for (auto device_channel : device->channels)
			if (channel == device_channel->structure)
				channels.push_back(device_channel);
	}
}

ChannelGroup::~ChannelGroup()
{
}

const string ChannelGroup::get_name()
{
	return valid_string(structure->name);
}

list<shared_ptr<Channel>> ChannelGroup::get_channels()
{
	list<shared_ptr<Channel>> result;
	for (auto channel : channels)
	{
		channel->parent = static_pointer_cast<Device>(parent->shared_from_this());
		result.push_back(shared_ptr<Channel>(channel, reset_parent<Channel>));
	}
	return result;
}

Glib::VariantBase ChannelGroup::config_get(const ConfigKey *key)
{
	GVariant *data;
	check(sr_config_get(parent->driver->structure, parent->structure,
		structure, key->get_id(), &data));
	return Glib::VariantBase(data);
}

void ChannelGroup::config_set(const ConfigKey *key, Glib::VariantBase value)
{
	check(sr_config_set(parent->structure, structure, key->get_id(), value.gobj()));
}

Glib::VariantBase ChannelGroup::config_list(const ConfigKey *key)
{
	GVariant *data;
	check(sr_config_list(parent->driver->structure, parent->structure,
		structure, key->get_id(), &data));
	return Glib::VariantBase(data);
}

CallbackData::CallbackData(Session *session, Callback callback) :
	callback(callback), session(session)
{
}

void CallbackData::run(const struct sr_dev_inst *sdi,
	const struct sr_datafeed_packet *pkt)
{
	auto device = session->devices[sdi];
	auto packet = shared_ptr<Packet>(new Packet(pkt), Packet::Deleter());
	callback(device, packet);
}

Session::Session(shared_ptr<Context> context) :
	structure(sr_session_new()), context(context)
{
	if (context->session != NULL)
		throw Error(SR_ERR_ARG);
	context->session = this;
}

Session::~Session()
{
	for (auto callback : callbacks)
		delete callback;
	check(sr_session_destroy());
}

void Session::add_device(shared_ptr<Device> device)
{
	check(sr_session_dev_add(device->structure));
	devices[device->structure] = device;
}

void Session::start()
{
	check(sr_session_start());
}

void Session::run()
{
	check(sr_session_run());
}

void Session::stop()
{
	check(sr_session_stop());
}

static void datafeed_callback(const struct sr_dev_inst *sdi,
	const struct sr_datafeed_packet *pkt, void *cb_data)
{
	auto callback = static_cast<CallbackData *>(cb_data);
	callback->run(sdi, pkt);
}
	
void Session::add_callback(Callback callback)
{
	auto cb_data = new CallbackData(this, callback);
	check(sr_session_datafeed_callback_add(datafeed_callback, cb_data));
	callbacks.push_back(cb_data);
}

Packet::Packet(const struct sr_datafeed_packet *structure) :
	structure(structure)
{
	switch (structure->type)
	{
		case SR_DF_LOGIC:
			payload = new Logic(
				static_cast<const struct sr_datafeed_logic *>(
					structure->payload));
			break;
		case SR_DF_ANALOG:
			payload = new Analog(
				static_cast<const struct sr_datafeed_analog *>(
					structure->payload));
			break;
		default:
			payload = NULL;
			break;
	}
}

Packet::~Packet()
{
	delete payload;
}

PacketPayload *Packet::get_payload()
{
	return payload;
}

PacketPayload::PacketPayload()
{
}

PacketPayload::~PacketPayload()
{
}

Logic::Logic(const struct sr_datafeed_logic *structure) : PacketPayload(),
	structure(structure),
	data(static_cast<uint8_t *>(structure->data),
		static_cast<uint8_t *>(structure->data) + structure->length) {}

Logic::~Logic()
{
}

void *Logic::get_data()
{
	return structure->data;
}

size_t Logic::get_data_size()
{
	return structure->length;
}

Analog::Analog(const struct sr_datafeed_analog *structure) :
	PacketPayload(),
	structure(structure)
{
}

Analog::~Analog()
{
}

void *Analog::get_data()
{
	return structure->data;
}

size_t Analog::get_data_size()
{
	return structure->num_samples * sizeof(float);
}

unsigned int Analog::get_num_samples()
{
	return structure->num_samples;
}

const Quantity *Analog::get_mq()
{
	return Quantity::get(structure->mq);
}

const Unit *Analog::get_unit()
{
	return Unit::get(structure->unit);
}

unordered_set<const QuantityFlag *> Analog::get_mqflags()
{
	return QuantityFlag::set_from_mask(structure->mqflags);
}

InputFormat::InputFormat(struct sr_input_format *structure) :
	StructureWrapper<Context, struct sr_input_format>(structure)
{
}

InputFormat::~InputFormat()
{
}

string InputFormat::get_name()
{
	return valid_string(structure->id);
}

string InputFormat::get_description()
{
	return valid_string(structure->description);
}

bool InputFormat::format_match(string filename)
{
	return structure->format_match(filename.c_str());
}

shared_ptr<InputFileDevice> InputFormat::open_file(string filename,
		map<string, string> options)
{
	auto input = g_new(struct sr_input, 1);

	/* Translate options to GLib hash table. */
	auto hash_table = g_hash_table_new_full(
		g_str_hash, g_str_equal, g_free, g_free);
	for (auto entry : options)
		g_hash_table_insert(hash_table,
			g_strdup(entry.first.c_str()),
			g_strdup(entry.second.c_str()));
	input->param = hash_table;

	/** Run initialisation. */
	check(structure->init(input, filename.c_str()));

	/** Create virtual device. */
	return shared_ptr<InputFileDevice>(new InputFileDevice(
		static_pointer_cast<InputFormat>(shared_from_this()), input, filename),
		InputFileDevice::Deleter());
}

InputFileDevice::InputFileDevice(shared_ptr<InputFormat> format,
		struct sr_input *input, string filename) :
	Device(input->sdi),
	input(input),
	format(format),
	filename(filename)
{
}

InputFileDevice::~InputFileDevice()
{
	g_hash_table_unref(input->param);
	g_free(input);
}

void InputFileDevice::load()
{
	check(format->structure->loadfile(input, filename.c_str()));
}

OutputFormat::OutputFormat(struct sr_output_format *structure) :
	StructureWrapper<Context, struct sr_output_format>(structure)
{
}

OutputFormat::~OutputFormat()
{
}

string OutputFormat::get_name()
{
	return valid_string(structure->id);
}

string OutputFormat::get_description()
{
	return valid_string(structure->description);
}

shared_ptr<Output> OutputFormat::create_output(shared_ptr<Device> device)
{
	return shared_ptr<Output>(
		new Output(
			static_pointer_cast<OutputFormat>(shared_from_this()), device),
		Output::Deleter());
}

shared_ptr<Output> OutputFormat::create_output(
	shared_ptr<Device> device, string option)
{
	return shared_ptr<Output>(
		new Output(
			static_pointer_cast<OutputFormat>(shared_from_this()),
				device, option),
		Output::Deleter());
}

Output::Output(shared_ptr<OutputFormat> format,
		shared_ptr<Device> device) :
	structure {format->structure, device->structure, NULL, NULL},
	format(format), device(device)
{
	check(format->structure->init(&structure));
}

Output::Output(shared_ptr<OutputFormat> format,
		shared_ptr<Device> device, string option) :
	structure {format->structure, device->structure,
		g_strdup(option.c_str()), NULL},
	format(format), device(device), option(option)
{
	check(format->structure->init(&structure));
}

Output::~Output()
{
	check(format->structure->cleanup(&structure));
	g_free(structure.param);
}

string Output::receive(shared_ptr<Packet> packet)
{
	uint8_t *output_buf;
	uint64_t output_len;
	bool using_obsolete_api = false;

	switch (packet->structure->type)
	{
		case SR_DF_TRIGGER:
		case SR_DF_FRAME_BEGIN:
		case SR_DF_FRAME_END:
		case SR_DF_END:
			if (format->structure->event)
			{
				check(format->structure->event(&structure,
					packet->structure->type,
					&output_buf, &output_len));
				using_obsolete_api = true;
			}
			break;
		default:
			break;
	}

	if (!using_obsolete_api &&
		packet->structure->type == format->structure->df_type)
	{
		check(format->structure->data(&structure,
			static_cast<uint8_t *>(packet->payload->get_data()),
			packet->payload->get_data_size(),
			&output_buf, &output_len));
		using_obsolete_api = true;
	}

	if (using_obsolete_api)
	{
		auto result = string(output_buf, output_buf + output_len);
		g_free(output_buf);
		return result;
	}

	if (format->structure->receive)
	{
		GString *out;
		check(format->structure->receive(&structure, device->structure,
			packet->structure, &out));
		if (out)
		{
			auto result = string(out->str, out->str + out->len);
			g_string_free(out, true);
			return result;
		}
	}

	return string();
}

ConfigInfo::ConfigInfo(const struct sr_config_info *structure) :
	structure(structure)
{
}

ConfigInfo::~ConfigInfo()
{
}

const ConfigKey *ConfigInfo::get_key()
{
	return ConfigKey::get(structure->key);
}

const DataType *ConfigInfo::get_datatype()
{
	return DataType::get(structure->datatype);
}

string ConfigInfo::get_id()
{
	return valid_string(structure->id);
}

string ConfigInfo::get_name()
{
	return valid_string(structure->name);
}

string ConfigInfo::get_description()
{
	return valid_string(structure->description);
}

#include "enums.cpp"

}
