    ConfigInfo get_info() const;
    static const ConfigKey *get(string name);
    Glib::VariantBase parse_string(string value) const;
