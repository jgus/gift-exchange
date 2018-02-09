
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <iostream>
#include <random>
#include <string>

template<typename TMap, typename TDefault = typename TMap::mapped_type>
auto map_get_or_default(TMap const& map, typename TMap::key_type const& key, TDefault const& defaultValue = TDefault())
{
    auto i = map.find(key);
    if (i == map.end())
        return defaultValue;
    return i->second;
};

typedef int family_id;
constexpr family_id base_family_id = 0;

struct person
{
    std::string name;
    int participation;
    family_id family;
};

typedef std::map<std::string, std::set<std::string>> assignments;

class assigner
{
public:
    void add_persons_from_file(std::string const& file_name)
    {
        boost::property_tree::ptree tree;
        boost::property_tree::read_json(file_name, tree);
        for (auto const& family_node : tree)
        {
            auto const familyId = next_family_id++;
            for (auto const& person_node : family_node.second)
            {
                auto const name = person_node.second.get<std::string>("name");
                auto const participation = person_node.second.get("p", 1);
                persons[name] = person { name, participation, familyId };
            }
        }
    }

    void add_forbidden_assignments_from_file(std::string const& file_name)
    {
        boost::property_tree::ptree tree;
        boost::property_tree::read_json(file_name, tree);
        for (auto const& person_node : tree)
        {
            auto const& name = person_node.first;
            for (auto const& target_node : person_node.second)
            {
                forbidden_assignments[name].insert(target_node.second.get_value<std::string>());
            }
        }
    }

    assignments generate_valid_assignments() const
    {
        std::random_device random_device;
        while (true)
        {
            assignments assignments = generate_assignments(random_device);
            if (is_valid(assignments))
                return assignments;
        }
    }

    void write_assignments_to_file(assignments const& assignments, std::string const& file_name) const
    {
        boost::property_tree::ptree tree;
        for (auto const& pair : assignments)
        {
            boost::property_tree::ptree person_tree;
            for (auto const& target : pair.second)
            {
                person_tree.push_back(make_pair("", boost::property_tree::ptree(target)));
            }
            tree.put_child(pair.first, person_tree);
        }
        boost::property_tree::json_parser::write_json(file_name, tree);
    }

private:
    family_id next_family_id = base_family_id;
    std::map<std::string, person> persons;
    assignments forbidden_assignments;

    template<typename TRandom>
    assignments generate_assignments(TRandom&& random_device) const
    {
        std::vector<std::string> targets;
        for (auto const& person : persons)
        {
            for (auto i = 0; i < person.second.participation; ++i)
            {
                targets.push_back(person.first);
            }
        }

        std::shuffle(std::begin(targets), std::end(targets), std::forward<TRandom>(random_device));

        assignments assignments;
        for (auto const& person : persons)
        {
            for (auto i = 0; i < person.second.participation; ++i)
            {
                assignments[person.first].insert(targets.back());
                targets.pop_back();
            }
        }

        return assignments;
    }

    bool is_valid(assignments const& assignments) const
    {
        std::set<std::pair<family_id, family_id>> family_assignments;

        for (auto const& person : persons)
        {
            auto const& source_name = person.first;
            auto const& source = person.second;
            auto forbidden_names = map_get_or_default(forbidden_assignments, source_name);
            auto target_names = map_get_or_default(assignments, source_name);

            if (target_names.size() != source.participation)
                return false; // Assigned the same person twice

            for (auto const& target_name : target_names)
            {
                auto const& target = persons.find(target_name)->second;

                if (forbidden_names.find(target_name) != forbidden_names.end())
                    return false; // Forbidden explicitly

                if (target.family == source.family)
                    return false; // Giving to own family

                auto target_target_names = map_get_or_default(assignments, target_name);

                if (target_target_names.find(source_name) != target_target_names.end())
                    return false; // Giving to own giver

                if (family_assignments.find(std::make_pair(source.family, target.family)) != family_assignments.end())
                    return false; // Someone in giver's family already giving to recipient's family

                family_assignments.insert(std::make_pair(source.family, target.family));
            }
        }

        return true;
    }
};

int main(int argc, char const* argv[])
{
    boost::program_options::options_description options;
    std::vector<std::string> persons_files;
    std::vector<std::string> forbidden_files;
    std::string output_file;
    options.add_options()
       ("help,h", "Show this usage message")
       ("persons,p", boost::program_options::value<std::vector<std::string>>(&persons_files)->required(), "Persons file(s)")
       ("forbidden,f", boost::program_options::value<std::vector<std::string>>(&forbidden_files), "Forbidden assignment file(s)")
       ("output,o", boost::program_options::value<std::string>(&output_file)->required(), "Output file")
    ;

    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, options), vm);
    boost::program_options::notify(vm);

    if (vm.count("help"))
    {
        std::cout << options << "\n";
        return 1;
    }

    assigner a;

    for (auto const& personsFile : persons_files)
    {
        a.add_persons_from_file(personsFile);
    }
    for (auto const& forbiddenFile : forbidden_files)
    {
        a.add_forbidden_assignments_from_file(forbiddenFile);
    }

    assignments generated_assignments = a.generate_valid_assignments();

    a.write_assignments_to_file(generated_assignments, output_file);

    return 0;
}
