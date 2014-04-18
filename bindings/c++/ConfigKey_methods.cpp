ConfigInfo ConfigKey::get_info() const
{
	return ConfigInfo(sr_config_info_get(id));
}

const ConfigKey *ConfigKey::get(string name)
{
	return ConfigInfo(sr_config_info_name_get(name.c_str())).get_key();
}

Glib::VariantBase ConfigKey::parse_string(string value) const
{
	GVariant *variant;
	uint64_t p, q;

	switch (get_info().get_datatype()->get_id())
	{
		case SR_T_UINT64:
			check(sr_parse_sizestring(value.c_str(), &p));
			variant = g_variant_new_uint64(p);
			break;
		case SR_T_CHAR:
			variant = g_variant_new_string(value.c_str());
			break;
		case SR_T_BOOL:
			variant = g_variant_new_boolean(sr_parse_boolstring(value.c_str()));
			break;
		case SR_T_FLOAT:
			variant = g_variant_new_double(stod(value));
			break;
		case SR_T_RATIONAL_PERIOD:
			check(sr_parse_period(value.c_str(), &p, &q));
			variant = g_variant_new("(tt)", p, q);
			break;
		case SR_T_RATIONAL_VOLT:
			check(sr_parse_voltage(value.c_str(), &p, &q));
			variant = g_variant_new("(tt)", p, q);
			break;
		case SR_T_INT32:
			variant = g_variant_new_int32(stoi(value));
			break;
		default:
			throw Error(SR_ERR_BUG);
	}

	return Glib::VariantBase(variant, true);
}

