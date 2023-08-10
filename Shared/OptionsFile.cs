﻿using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Shared
{
    public class OptionsFile
    {
        private string m_FileName;
        private string[] m_CompatFileNames;
        private Dictionary<string, string> m_Options = new Dictionary<string, string>();

        public OptionsFile(string fileName, params string[] compatFileNames)
        {
            m_FileName = fileName;
            m_CompatFileNames = compatFileNames;
        }

        public bool HasKey(params string[] keys)
        {
            foreach (var key in keys)
            {
                if (m_Options.ContainsKey(key))
                {
                    return true;
                }
            }

            return false;
        }

        public IEnumerable<string> GetKeys()
        {
            foreach (var option in m_Options)
            {
                yield return option.Key;
            }
        }

        public string ReadValue(string key, string defaultValue = null, params string[] compatKeys)
        {
            var keys = compatKeys.Concat(new [] { key });
            foreach (var _key in keys)
            {
                if (m_Options.ContainsKey(_key))
                {
                    return m_Options[_key];
                }
            }

            return defaultValue;
        }

        public int ReadValueInt(string key, int defaultValue, params string[] compatKeys)
        {
            int result;
            if (!int.TryParse(ReadValue(key, null, compatKeys), out result))
            {
                result = defaultValue;
            }

            return result;
        }

        public bool ReadValueBool(string key, bool defaultValue, params string[] compatKeys)
        {
            bool result = defaultValue;
            if (HasKey(key))
            {
                if (int.TryParse(ReadValue(key, null, compatKeys), out int tmp))
                {
                    result = tmp != 0;
                }
            }

            return result;
        }

        public void WriteValue(string key, string value)
        {
            if (value != null && value.Trim().Length > 0)
            {
                m_Options[key] = value;
            }
            else
            {
                m_Options.Remove(key);
            }
        }

        public void WriteValue(string key, int value)
        {
            WriteValue(key, $"{value}");
        }

        public void WriteValue(string key, bool value)
        {
            WriteValue(key, value ? 1 : 0);
        }

        public void ReadFile()
        {
            string readData(string fileName)
            {
                if (!File.Exists(fileName))
                {
                    return null;
                }

                string _data = File.ReadAllText(fileName);
                if (_data.Length == 0)
                {
                    return null;
                }

                return _data;
            }

            string data;
            if ((data = readData(m_FileName)) == null)
            {
                bool dataRead = false;
                foreach (var compatFileName in m_CompatFileNames)
                {
                    if ((data = readData(compatFileName)) != null)
                    {
                        dataRead = true;
                        break;
                    }
                }

                if (!dataRead)
                {
                    return;
                }
            }

            m_Options.Clear();

            foreach (string line in data.Split('\n'))
            {
                var keyValuePair = line.Split('=');
                if (keyValuePair.Length != 2)
                {
                    continue;
                }

                m_Options[keyValuePair[0].Trim()] = keyValuePair[1].Trim();
            }
        }

        public void WriteFile()
        {
            string data = string.Empty;

            foreach (var option in m_Options)
            {
                data += $"{option.Key}={option.Value}\n";
            }

            Directory.CreateDirectory(Path.GetDirectoryName(m_FileName));
            File.WriteAllText(m_FileName, data);
        }

        public void ResetFile()
        {
            Directory.CreateDirectory(Path.GetDirectoryName(m_FileName));
            File.WriteAllText(m_FileName, null);

            // Reset all options too
            m_Options = new Dictionary<string, string>();
        }

        public bool HasCompatFile()
        {
            foreach (var compatFileName in m_CompatFileNames)
            {
                if (File.Exists(compatFileName))
                {
                    return true;
                }
            }

            return false;
        }

        public bool HasCompatFile(string fileName)
        {
            return m_CompatFileNames.Contains(fileName) && File.Exists(fileName);
        }

        public string[] GetCompatFiles()
        {
            return m_CompatFileNames;
        }
    }
}
