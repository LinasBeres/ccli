/*
MIT License

Copyright(c) 2022 Lukas Lipp

Permission is hereby granted, free of charge, to any person obtaining a copy
of this softwareand associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright noticeand this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <string>
#include <array>
#include <functional>
#include <sstream>
#include <deque>

class ccli
{
public:
	static void						parseArgs(int aArgc, const char* const aArgv[]);
	static void						loadConfig(const std::string& aCfgFile);
	static void						writeConfig(const std::string& aCfgFile);
	static void						executeCallbacks();
	static std::deque<std::string>	getHelp();
	static std::deque<std::string>  checkErrors();

	enum Flag {
		NONE						= (0 << 0),
		CLI_ONLY					= (1 << 0),	// can only be set through parseArgs
		READ_ONLY					= (1 << 1),	// display only, cannot be changed at all
		CONFIG_RD					= (1 << 2),	// load variable from config file
		CONFIG_RDWR					= (3 << 2),	// load variable from config file and save changes back to config file
		MANUAL_EXEC					= (1 << 4)	// execute callback only when executeCallback/executeCallbacks is called
	};

	class var_base
	{
	public:
									var_base(std::string aLongName, std::string aShortName, std::string aDescription, bool aSingleBool, bool aHasCallback);
		virtual						~var_base();
									
									var_base(const var_base&) = delete;
									var_base(var_base&&) = delete;
		var_base&					operator=(const var_base&) = delete;
		var_base&					operator=(var_base&&) = delete;

		const std::string&			getLongName() const;
		const std::string&			getShortName() const;
		const std::string&			getDescription() const;

		virtual std::string			getValueString() = 0;
		virtual void				setValueString(const std::string& aString) = 0;

		bool						isSingleBool() const;

		bool						hasCallback() const;
		virtual bool				executeCallback() = 0;

		virtual bool				isCliOnly() const = 0;
		virtual bool				isReadOnly() const = 0;
		virtual bool				isConfigRead() const = 0;
		virtual bool				isConfigReadWrite() const = 0;
		virtual bool				isCallbackAutoExecuted() const = 0;

		virtual bool				isBool() const = 0;
		virtual bool				isInt() const = 0;
		virtual bool				isFloat() const = 0;
		virtual bool				isString() const = 0;

		bool						locked() const;
		void						locked(bool aLocked);

	protected:
		virtual void				setValueStringInternal(const std::string& aString) = 0;

		friend class ccli;
		const std::string			mLongName;
		const std::string			mShortName;
		const std::string			mDescription;
		const bool					mSingleBool;
		const bool					mHasCallback;
		bool						mLocked;
	};

	template <class T, size_t S = 1, uint32_t F = NONE>
	class var final : public var_base {
		static_assert(std::is_same_v<T, int> || std::is_same_v<T, float> || std::is_same_v<T, bool> || std::is_same_v<T, std::string>, "Type must be bool, int, float or string");
		static_assert(S >= 1, "Size must be larger 0");
	public:
							var(const std::string& aLongName, const std::string& aShortName, const std::string& aDescription, 
								const std::array<T, S>& aValue = {}, const std::function<void(const std::array<T, S>&)> aCallback = {})
								: var_base(aLongName, aShortName, aDescription, std::is_same_v<T, bool> && S == 1, aCallback != nullptr),
								mCallback(aCallback), mCallbackCharged(false), mValue(aValue) {}
							~var() override = default;

							var(const var&) = delete;
							var(var&&) = delete;
		var&				operator=(const var&) = delete;
		var&				operator=(var&&) = delete;

		std::array<T, S>&	getValue() { return mValue; }
		void				setValue(const std::array<T, S>& aValue)
							{
								if (isCliOnly()) return;
								setValueInternal(aValue);
							}

		void				chargeCallback() { mCallbackCharged = true; }
		bool				executeCallback() override
							{
								if (hasCallback() && mCallbackCharged) {
									mCallback(mValue);
									mCallbackCharged = false;
									return true;
								}
								return false;
							}

		bool				isCliOnly() const override { return F & CLI_ONLY; }
		bool				isReadOnly() const override { return F & READ_ONLY; }
		bool				isConfigRead() const override { return !mLongName.empty() && F & CONFIG_RD; }
		bool				isConfigReadWrite() const override { return !mLongName.empty() && F & CONFIG_RDWR; }
		bool				isCallbackAutoExecuted() const override { return !(F & MANUAL_EXEC); }
		bool				isCallbackCharged() const { return mCallbackCharged; }

		bool				isBool() const override { return std::is_same_v<T, bool>; }
		bool				isInt() const override { return std::is_same_v<T, int>; }
		bool				isFloat() const override { return std::is_same_v<T, float>; }
		bool				isString() const override { return std::is_same_v<T, std::string>; }

		void				setValueString(const std::string& aString) override
							{
								if (isCliOnly()) return;
								setValueStringInternal(aString);
							}
		std::string			getValueString() override
							{
								std::string s;
								for(const T& v : mValue)
								{
									if (!s.empty()) s += mDelimiter;
									if constexpr (std::is_same_v<T, std::string>) s += v;
									else s += std::to_string(v);
								}
								return s;
							}
	private:
		void				setValueInternal(const std::array<T, S>& aValue)
							{
								if (mLocked || isReadOnly()) return;
								mValue = aValue;
								if (hasCallback()) {
									mCallbackCharged = true;
									if (isCallbackAutoExecuted()) executeCallback();
								}
							}
		void				setValueStringInternal(const std::string& aString) override
							{
								if (mLocked || isReadOnly()) return;
								// empty string only allowed for bool and string
								if constexpr (!std::is_same_v<T, bool> && !std::is_same_v<T, std::string>) if (aString.empty()) return;

								size_t count = 0;
								size_t current = 0;
								size_t pos;
								do {
									pos = aString.find(mDelimiter, current);
									std::string token = aString.substr(current, pos - current);
									current = pos + 1;

									if (count > mValue.size() - 1) break;

									if constexpr (std::is_same_v<T, float>) mValue.at(count) = std::stof(token);
									else if constexpr (std::is_same_v<T, int>) mValue.at(count) = std::stoi(token);
									else if constexpr (std::is_same_v<T, std::string>) mValue.at(count) = token;
									else if constexpr (std::is_same_v<T, bool>) {
										if (token.empty()) mValue.at(count) = true;
										else {
											bool bn, bs;
											std::istringstream(token) >> bn;
											std::istringstream(token) >> std::boolalpha >> bs;
											mValue[count] = bn || bs;
										}
									}
									count++;
								} while (pos != std::string::npos);

								mCallbackCharged = true;
								if (isCallbackAutoExecuted()) executeCallback();
							}

		const std::string	mDelimiter = ",";
		const std::function<void(const std::array<T, S>&)> mCallback;
		bool				mCallbackCharged;
		std::array<T, S>	mValue;
	};
};
