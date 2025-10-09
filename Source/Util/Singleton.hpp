#pragma once

template <typename T>
class TSingleton
{
public:
	static T& GetInstance()
	{
		static T Instance{};
		return Instance;
	}

protected:
	TSingleton() = default;
	virtual ~TSingleton() = default;
};
