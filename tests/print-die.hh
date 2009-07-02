
static void
print_die_main (int &argc, char **&argv, unsigned int &depth)
{
  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  (void) bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  /* Initialize the message catalog.  */
  (void) textdomain (PACKAGE_TARNAME);

  cout << hex << setiosflags (ios::showbase);

  depth = 0;
  if (argc > 1 && sscanf (argv[1], "--depth=%u", &depth) == 1)
    {
      --argc;
      ++argv;
    }
}

template<typename file>
static void
print_die (const typename file::debug_info_entry &die,
	   unsigned int indent, unsigned int limit)
{
  string prefix (indent, ' ');
  const string tag = dwarf::tags::name (die.tag ());

  cout << prefix << "<" << tag << " offset=[" << die.offset () << "]";

  for (typename file::debug_info_entry::attributes_type::const_iterator i
	 = die.attributes ().begin (); i != die.attributes ().end (); ++i)
    cout << " " << to_string (*i);

  if (die.has_children ())
    {
      if (limit != 0 && indent >= limit)
	{
	  cout << ">...\n";
	  return;
	}

      cout << ">\n";

      for (typename file::debug_info_entry::children_type::const_iterator i
	     = die.children ().begin (); i != die.children ().end (); ++i)
	print_die<file> (*i, indent + 1, limit);

      cout << prefix << "</" << tag << ">\n";
    }
  else
    cout << "/>\n";
}

template<typename file>
static void
print_cu (const typename file::compile_unit &cu, const unsigned int limit)
{
  print_die<file> (static_cast<const typename file::debug_info_entry &> (cu),
		   1, limit);
}

template<typename file>
static void
print_file (const char *name, const file &dw, const unsigned int limit)
{
  cout << name << ":\n";

  for (typename file::compile_units::const_iterator i
	 = dw.compile_units ().begin (); i != dw.compile_units ().end (); ++i)
    print_cu<file> (*i, limit);
}
