namespace uaglbr
{
using bytes = std::vector<unsigned char>;

static const unsigned char lower[26] = {
  0x01, 0x03, 0x09, 0x19, 0x11, 0x0b, 0x1b, 0x13, 0x0a, 0x1a,
  0x05, 0x07, 0x0d, 0x1d, 0x15, 0x0f, 0x1f, 0x17, 0x0e, 0x1e,
  0x25, 0x27, 0x3a, 0x2d, 0x3d, 0x35
};

static int cell (unsigned char c)
{
  if (c >= 'a' && c <= 'z') return lower[c - 'a'];
  if (c >= 'A' && c <= 'Z') return lower[c - 'A'] + 0x40;
  if (c >= '0' && c <= '9') return lower[(c - '0' + 9) % 10] + 0x80;
  return -1;
}

static bytes encode (const bytes &input)
{
  bytes output;
  for (unsigned char c : input)
    {
      int p = cell (c);
      if (p < 0)
        output.push_back (c);
      else
        {
          output.push_back (0xe2);
          output.push_back (0xa0 | (p >> 6));
          output.push_back (0x80 | (p & 63));
        }
    }
  return output;
}

static bytes decode (const bytes &input)
{
  bytes output;
  for (size_t i = 0; i < input.size (); )
    {
      int c = -1;
      if (i + 2 < input.size () && input[i] == 0xe2
          && (input[i + 1] & 0xfc) == 0xa0
          && (input[i + 2] & 0xc0) == 0x80)
        {
          unsigned p = ((input[i + 1] & 3) << 6) | (input[i + 2] & 63);
          for (unsigned j = 0; j < 26; ++j)
            {
              if (p == lower[j]) c = 'a' + j;
              if (p == lower[j] + 0x40u) c = 'A' + j;
            }
          for (unsigned j = 0; j < 10; ++j)
            if (p == lower[(j + 9) % 10] + 0x80u) c = '0' + j;
        }
      if (c >= 0)
        {
          output.push_back (c);
          i += 3;
        }
      else
        output.push_back (input[i++]);
    }
  return output;
}

static bytes read (const char *path)
{
  FILE *f = fopen (path, "rb");
  if (!f) fatal_error (input_location, "UAGLBR cannot read %qs: %m", path);
  bytes data;
  unsigned char buffer[65536];
  size_t n;
  while ((n = fread (buffer, 1, sizeof buffer, f)) != 0)
    data.insert (data.end (), buffer, buffer + n);
  int failed = ferror (f);
  failed |= fclose (f) != 0;
  if (failed) fatal_error (input_location, "UAGLBR read failed for %qs", path);
  return data;
}

static void write (const char *path, const bytes &data)
{
  FILE *f = !strcmp (path, "-") ? stdout : fopen (path, "wb");
  if (!f) fatal_error (input_location, "UAGLBR cannot write %qs: %m", path);
  size_t n = 0;
  while (n < data.size ())
    {
      size_t k = fwrite (data.data () + n, 1, data.size () - n, f);
      if (!k) fatal_error (input_location, "UAGLBR write failed for %qs", path);
      n += k;
    }
  int failed = f == stdout ? fflush (f) : fclose (f);
  if (failed) fatal_error (input_location, "UAGLBR close failed for %qs", path);
}

static char *temporary (const char *suffix)
{
  char *path = make_temp_file (suffix);
  record_temp_file (path, 1, 1);
  return path;
}

static void command (const char *tool, std::vector<const char *> arguments)
{
  char *program = find_a_program (tool);
  if (!program) fatal_error (input_location, "UAGLBR cannot find %qs", tool);
  arguments.insert (arguments.begin (), program);
  arguments.push_back (nullptr);
  int status = 0, err = 0;
  const char *message = pex_one (PEX_SEARCH, program,
                               const_cast<char *const *> (arguments.data ()),
                               progname, nullptr, nullptr, &status, &err);
  if (message)
    fatal_error (input_location, "UAGLBR could not run %qs: %s", tool, message);
  free (program);
  if (status)
    fatal_error (input_location, "UAGLBR %qs stage failed", tool);
}

static std::string asm_quote (const char *path)
{
  std::string result = "\"";
  for (const unsigned char *p = (const unsigned char *)path; *p; ++p)
    {
      if (*p < 32 || *p >= 127)
        {
          char octal[5];
          snprintf (octal, sizeof octal, "\\%03o", *p);
          result += octal;
        }
      else
        {
          if (*p == '\\' || *p == '"') result += '\\';
          result += *p;
        }
    }
  return result + "\"";
}

static void package (const char *object)
{
  bytes native = read (object);
  if (native.size () < 20 || native[0] != 0x7f || native[1] != 'E'
      || native[2] != 'L' || native[3] != 'F' || native[5] != 1)
    fatal_error (input_location, "UAGLBR requires a little-endian ELF object");
  unsigned machine = native[18] | (native[19] << 8);
  const char *as_mode = nullptr, *ld_mode = nullptr, *runtime_name = nullptr;
  if (native[4] == 2 && machine == 62)
    {
      as_mode = "--64"; ld_mode = "elf_x86_64";
      runtime_name = "uaglbr-runtime.o";
    }
  else if (native[4] == 1 && machine == 3)
    {
      as_mode = "--32"; ld_mode = "elf_i386";
      runtime_name = "uaglbr-runtime32.o";
    }
  else if (native[4] == 1 && machine == 62)
    {
      as_mode = "--x32"; ld_mode = "elf32_x86_64";
      runtime_name = "uaglbr-runtimex32.o";
    }
  else
    fatal_error (input_location, "UAGLBR runtime is built for the x86 ELF targets");
  char *runtime = find_a_file (&startfile_prefixes, runtime_name, false);
  if (!runtime)
    fatal_error (input_location, "UAGLBR cannot find %qs", runtime_name);
  static const char hex[] = "0123456789abcdef";
  bytes hexadecimal;
  for (unsigned char c : native)
    {
      hexadecimal.push_back (hex[c >> 4]);
      hexadecimal.push_back (hex[c & 15]);
    }
  bytes payload = encode (hexadecimal);
  char *payload_file = temporary (".uaglbr.payload");
  char *assembly_file = temporary (".uaglbr.s");
  char *payload_object = temporary (".uaglbr.o");
  char *linked_object = temporary (".uaglbr.link.o");
  write (payload_file, payload);
  FILE *assembly = fopen (assembly_file, "wb");
  if (!assembly) fatal_error (input_location, "UAGLBR cannot open payload assembly");
  fprintf (assembly,
           ".section .uaglbr.payload,\"a\",@progbits\n"
           ".Luaglbr_payload:\n.incbin %s\n"
           ".section .uaglbr.meta,\"a\",@progbits\n.balign 8\n"
           ".long 1\n.long %u\n.quad %llu\n.quad %llu\n",
           asm_quote (payload_file).c_str (), machine,
           (unsigned long long) native.size (),
           (unsigned long long) payload.size ());
  if (native[4] == 2)
    fputs (".quad .Luaglbr_payload-.\n", assembly);
  else
    fputs (".long .Luaglbr_payload-.\n.long 0\n", assembly);
  fputs (".section .note.GNU-stack,\"\",@progbits\n", assembly);
  if (fclose (assembly)) fatal_error (input_location, "UAGLBR payload write failed");
  command ("as", {as_mode, "-o", payload_object, assembly_file});
  command ("ld", {"-m", ld_mode, "-r", "-o", linked_object,
                  object, payload_object, runtime});
  write (object, read (linked_object));
  free (runtime);
  free (payload_file);
  free (assembly_file);
  free (payload_object);
  free (linked_object);
}
}

static const char *
uaglbr_prepare_spec_function (int argc, const char **argv)
{
  if (verbose_only_flag) return nullptr;
  if (argc != 2) fatal_error (input_location, "UAGLBR assembly stage requires two paths");
  uaglbr::bytes encoded = uaglbr::encode (uaglbr::read (argv[0]));
  uaglbr::write (argv[1], encoded);
  return nullptr;
}

static const char *
uaglbr_save_spec_function (int argc, const char **argv)
{
  if (verbose_only_flag) return nullptr;
  if (argc != 2) fatal_error (input_location, "UAGLBR assembly output requires two paths");
  uaglbr::write (argv[1], uaglbr::encode (uaglbr::read (argv[0])));
  return nullptr;
}

static const char *
uaglbr_load_spec_function (int argc, const char **argv)
{
  if (verbose_only_flag) return nullptr;
  if (argc != 2) fatal_error (input_location, "UAGLBR assembly input requires two paths");
  uaglbr::bytes input = uaglbr::read (argv[0]);
  size_t n = strlen (argv[0]);
  if (n >= 7 && !strcmp (argv[0] + n - 7, ".uaglbr"))
    input = uaglbr::decode (input);
  uaglbr::write (argv[1], input);
  return nullptr;
}

static const char *
uaglbr_object_spec_function (int argc, const char **argv)
{
  if (verbose_only_flag) return nullptr;
  if (argc != 1) fatal_error (input_location, "UAGLBR object stage requires one path");
  if (!strcmp (argv[0], HOST_BIT_BUCKET)) return nullptr;
  uaglbr::package (argv[0]);
  return nullptr;
}
