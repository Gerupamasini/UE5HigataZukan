// A small JSON reader for the game data (Content/Data/*.json). Plain C++17 so the standalone tests can load the
// same files the game does. Objects keep their key order; strings are UTF-8.

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace HigataSim
{
	class FJson
	{
	public:
		enum class EType { Null, Bool, Number, String, Array, Object };

		EType Type = EType::Null;
		bool Bool = false;
		double Number = 0.0;
		std::string String;
		std::vector<FJson> Items;
		std::vector<std::pair<std::string, FJson>> Members;

		/** Parse a document; on failure returns false and sets Error. */
		static bool Parse(const std::string& Text, FJson& Out, std::string& Error);
		static bool ParseFile(const std::string& Path, FJson& Out, std::string& Error);

		bool IsNull() const { return Type == EType::Null; }
		bool IsNumber() const { return Type == EType::Number; }
		bool IsString() const { return Type == EType::String; }
		bool IsArray() const { return Type == EType::Array; }
		bool IsObject() const { return Type == EType::Object; }

		/** Member by key, or nullptr (also when this is not an object). */
		const FJson* Get(const std::string& Key) const;

		double NumberOr(const std::string& Key, double Default) const;
		std::string StringOr(const std::string& Key, const std::string& Default) const;
		bool BoolOr(const std::string& Key, bool Default) const;
		/** An array of strings (non-strings skipped); empty when missing. */
		std::vector<std::string> Strings(const std::string& Key) const;
	};
}
