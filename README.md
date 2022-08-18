> **注意**，我在项目内使用了微软2022.8.18最新的DirectXTK工具，如果你碰到问题，你可以在[GitHub_DirectXTK](https://github.com/microsoft/DirectXTK12)找到最新的内容。

> 以下是如何使用这个DirectXTK工具项目：
* 在您的应用程序的解决方案中，右键单击解决方案并使用Add \ Existing Project...将适当的.vcxproj文件添加到您的解决方案中。
* 在您的应用程序的项目中，右键单击项目并使用“References...”，然后选择“Add New Reference...”，然后检查 DirectXTK12 项目名称并单击 OK。
* 在您的应用程序的项目设置中，在“C++ / General”页面上将 Configuration 设置为“All Configurations”，将 Platform 设置为“All Platforms”，然后将相对路径添加到 --DirectXTK12\inc;假设您在同一目录中有 DirectXTK12 文件夹您的sln文件，它应该是$(SolutionDir)\DirectXTK12\inc;--附加包含目录属性。单击应用。
[具体内容请查看Wiki](https://github.com/microsoft/DirectXTK12/wiki/DirectXTK#adding-to-a-vs-solution)