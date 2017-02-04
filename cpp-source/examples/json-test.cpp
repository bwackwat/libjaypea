#include <iostream>

#include "json.hpp"
#include "util.hpp"

static void test(std::string str){
	JsonObject object;
	std::string objectString;
	JsonObject verify;
	std::string verifyString;

	std::cout << "----------------------------------------------\n";
	std::cout << "Parsing:\n" << str << '\n';

	object.parse(str.c_str());
	objectString = object.stringify();
	std::cout << "Stringified: " << JsonObject::typeString[object.type] << '\n' << objectString << '\n';

	std::cout << "Pretty: " << JsonObject::typeString[object.type] << '\n' << object.stringify(true) << '\n';

	verify.parse(object.stringify().c_str());
	verifyString = verify.stringify();
	std::cout << "Verify: " << JsonObject::typeString[verify.type] << '\n' <<  verify.stringify(true) << '\n';

	//std::cout << "\n\n" << objectString << "\n\n" << verifyString << "\n\n";

	if(Util::strict_compare_inequal(objectString.c_str(), verifyString.c_str())){
		std::cout << "Verification Failure...\n";
	}else if(objectString.length() != verifyString.length()){
		std::cout << "Verification Failure CHECK LENGTH...\n";
	}else{
		std::cout << "Verification Success!\n";
	}
}

int main(){
	test("   \"apple\"  ");
	test(" {  \"best fruit\"   :    \"apple\"  } ");
	test(" { \"best fruit\" : { \"first\" : \"apple\" ,  \"second\" : \"banana\" } } ");
	test("{\"what about oranges?\":{}}");
	test("[\"apples\",\"bananas\",{\"oranges\":{\"they are\":[\"orange\",\"sweet\",[\"circle\",\"round\",\"sphere\"]]}}]");
	test(" EminemVEVO\
19M\
167,625,010 views\
Uploaded on Dec 24, 2009\
\
Music video by Eminem performing ");
	test("[\"apples\",\"bananas\",{\"oranges\":{\"they are\":[\"orange\",\"sweet\",[\"circle\",\"round\",\"sphere]]}}]");
	test("[\"apples{}[]\"]");
	test("[\"apples,\"bananas\",{\"oranges\":{\"they are\":[\"orange\",\"sweet\",[\"circle\",\"round\",\"sphere\"]]}}]");
	test("{\"result\":\"Welcome to the API\",\"routes\":{\"\\\":[],\"login\":[\"username\"]}}");
	test("{\"allcharacters\\\"{;\\};[,\\],aredone\":\"success\"}");
	test("{\"allc\\\\haracters\\\"{;\\};[,\\],aredone\":\"success\"}");
	test("{\"postcontent\":\"build script with a special keyword indicating the production environment.</li></ul>\n<br />It was really an excellent experience, and now I can test new apps/features on my delicious little BSD VM and can expect redeployments on my server to be safe. This post was submitted in PC-BSD. It was a good lesson in production, and as I progress with my framework, everything will continue getting fine-tuned for the development environment and for production deployments. I hope this was at least a little enlightening. Cheers!\",\"created_on\":\"2015-06-12 15:38:14\",{\"id\":\"25\",\"title\":\"Abstraction or \\\"Artificial Intelligence\\\"? A Different Industry.\",\"content\":\"If you hop over to the Wikipedia page for \\\"Applications of Artificial Intelligence\\\" you'll find the text: \\\"...all of the following were originally developed in AI laboratories: time sharing, interactive interpreters, graphical user interfaces and the computer mouse, rapid development environments, the linked list data structure, automatic storage management, symbolic programming, functional programming, dynamic programming and object-oriented programming.\\\" Even OOP was developed in an \\\"AI\\\" laboratory? This is not the modern, novel conception.<br />\n<br />\nRather, I propose that artificial intelligence has simply provided <i>very effective</i> abstractions which work well for us. The computer mouse? Well sure, once you've made the truly proper analysis of the necessities of a human-computer interface, the mouse becomes obvious. Nonetheless, it is an\"}");
	test("{\"a\":\"a\",\"b\":{\"c\":[{\"d\":\"d\",\"e\":[\"f\",\"f\",\"g\":{\"h\":\"h\"}],\"i\":{\"j\":\"j\",\"k\":{\"l\":{\"m\"{\"n\":\"n\",\"o\":{\"p\":[]}},\"q\":\"q\"},\"r\":\"r\"},\"s\":\"s\"},\"t\":\"t\"}],\"u\":\"u\"}}");
	test("{\"items\":[{\"category\":\"News\",\"description\":\"Google Maps API Introduction ...\",\"id\":\"hYB0mn5zh2c\",\"tags\":[\"GDD07\",\"GDD07US\",\"Maps\"],\"thumbnail\":{\"default\":\"http://i.ytimg.com/vi/hYB0mn5zh2c/default.jpg\",\"hqDefault\":\"http://i.ytimg.com/vi/hYB0mn5zh2c/hqdefault.jpg\",\"player\":{\"content\":{\"1\":\"rtsp://v5.cache3.c.youtube.com/CiILENy.../0/0/0/video.3gp\",\"5\":\"http://www.youtube.com/v/hYB0mn5zh2c?f...\",\"6\":\"rtsp://v1.cache1.c.youtube.com/CiILENy.../0/0/0/video.3gp\",\"aspectRatio\":\"widescreen\",\"commentCount\":\"22\",\"duration\":\"2840\",\"favoriteCount\":\"201\",\"rating\":\"4.63\",\"ratingCount\":\"68\",\"status\":{\"accessConate\":\"allowed\",\"comment\":\"allowed\",\"commentVote\":\"allowed\",\"default\":\"http://www.youtube.com/watch?vu003dhYB0mn5zh2c\",\"embed\":\"allowed\",\"list\":\"allowed\",\"rate\":\"allowed\",\"reason\":\"limitedSyndication\",\"value\":\"restricted\",\"videoRespond\":\"moderated\",\"viewCount\":\"220101\"},\"title\":\"Google Developers Day US - Maps API Introduction\",\"updated\":\"2010-01-07T13:26:50.000Z\",\"uploaded\":\"2007-06-05T22:07:03.000Z\",\"uploader\":\"GoogleDeveloperDay\"}}}}]}");
	test("({\"n\":\"0\",\"a\":{\"d\":\"Z\",\"x\":\"1\",\"e\":\"1\",\"s\":[{\"y\":\"s\",\"e\":\"n\",\"n\":\".\",\"s\":[\"7\",\"S\",\"s\"],\"l\":{\"t\":\"g\",\"t\":\"g\"},\"r\":{\"t\":\"c\"},\"t\":{\"1\":\"p\",\"5\":\".\",\"6\":\"p\"},\"n\":\"0\",\"o\":\"e\",\"g\":\"3\",\"t\":\"8\",\"t\":\"1\",\"t\":\"2\",\"t\":\"2\",\"s\":{\"e\":\"d\",\"n\":\"n\"},\"e\":\"d\",\"e\":\"d\",\"e\":\"d\",\"t\":\"d\",\"t\":\"d\",\"d\":\"d\",\"d\":\"d\"}}]}})");
	test("({\"n\":\"0\",\"a\":{\"d\":\"Z\",\"x\":\"1\",\"e\":\"1\",\"s\":[{\"y\":\"s\",\"l\":{\"t\":\"g\"},\"r\":{\"t\":\"c\"},\"n\":\"0\",\"o\":\"e\",\"g\":\"3\",\"s\":{\"e\":\"d\",\"n\":\"n\"},\"e\":\"d\",\"t\":\"d\",\"d\":\"d\"}}]}})");
	test("({\"b\":{\"items\":[{\"tags\":[\"Maps\"],\"thumbnail\":{\"hqDefault\":\"hault.jpg\"},\"player\":{\"default\":\"http:/5zh2c\"}}})");
	test("({\"data\":{\"items\":[{\"tags\":[\"Maps\"],\"thumbnail\":{\"hqDefault\":\"http://i.ytimg.com\"},\"player\":{\"content\":{\"6\":\"rtsp://v1.cache1.c.youtube.com/CiILENy.../0/0/0/video.3gp\"},\"status\":{\"reason\":\"limitedSyndication\"},\"videoRespond\":\"moderated\"}}]}})");

	std::cout << "----------------------------------------------\n";
	std::cout << "Done!\n";

	return 0;
}	
