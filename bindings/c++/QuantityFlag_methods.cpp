std::unordered_set<const QuantityFlag *> 
    QuantityFlag::set_from_mask(unsigned int mask)
{
    auto result = std::unordered_set<const QuantityFlag *>();
    while (mask)
    {
        unsigned int new_mask = mask & (mask - 1);
        result.emplace(QuantityFlag::get(
            static_cast<enum sr_mqflag>(mask ^ new_mask)));
        mask = new_mask;
    }
    return result;
}
